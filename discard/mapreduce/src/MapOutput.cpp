#include "MapOutput.h"
#include <terark/io/MemStream.hpp>
#include <terark/trb_cxx.hpp>
#include <assert.h>

#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h> // writev
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <fcntl.h>
#include <errno.h>

#include <queue>

#include <boost/thread.hpp>
#include <boost/bind.hpp>

#include "Partitioner.h"

using namespace terark;
using namespace std;

class MrPipe
{
protected:
	// `nr` --- Number Record
	size_t nrFirst;
	size_t nrCurr;
	size_t nrFlushed;

public:
	MapOutput_impl* owner;
	string reduceTaskID;
	int    iReduce;
	int    bufsize;
	int    sockfd;

	MrPipe()
	{
		sockfd    = -1;
		nrFirst   = 0;
		nrCurr    = 0;
		nrFlushed = 0;
		bufsize = 0;
		owner = 0;
	}

	virtual ~MrPipe()
	{
		if (-1 != sockfd)
			::close(sockfd);
	}

	virtual void write(const void* key, size_t klen, const void* val, size_t vlen) = 0;
	virtual void flush() = 0;

	virtual bool isClean() const { return nrFlushed == nrCurr; }
};

class MapOutput_impl
{
protected:
	vector<MrPipe*> partitions;
	Partitioner* partitioner;
	int nReduce;
	int bufsize;

public:
	explicit MapOutput_impl(MapContext* context)
	{
		long minbuf =         1024;
		long maxbuf = 64*1024*1024;

		bufsize = context->getll("MapOutput.backup.bufsize", 8*1024);
		if (bufsize < minbuf) bufsize = minbuf;
		if (bufsize > maxbuf) bufsize = maxbuf;

		nReduce = context->getll("nReduce", 1);
		if (nReduce < 0) {
			fprintf(stderr, "invalid config: nReduce=%d\n", nReduce);
			exit(1);
		}
		partitions.resize(nReduce);
		if (1 == nReduce) {
			partitioner = NULL;
		}
		else {
			partitioner = new Partitioner;
		}
	}

	virtual ~MapOutput_impl()
	{
		for (ptrdiff_t i = 0, n = partitions.size(); i < n; ++i) {
			MrPipe* pipe = partitions[i];
			assert(pipe->isClean());
			delete pipe;
		}
	}

	void write(const void* key, size_t klen, const void* val, size_t vlen)
	{
		int nth = partitioner->hash(key, klen, nReduce);
		assert(nth < nReduce);
		partitions[nth]->write(key, klen, val, vlen);
	}

	void flush()
	{
		for (ptrdiff_t i = 0, n = partitions.size(); i < n; ++i) {
			partitions[i]->flush();
		}
	}

	virtual void wait() = 0;

	pair<int, string> createBackupFile(int partitionIdx, MapContext* context)
   	{
		string prefix  = context->get("map.temp.dir", "/tmp/map-temp-dir");
		string jobname = context->get("mapreduce.jobname", "");
		AutoGrownMemIO fname;
		fname.printf("%s_%s_%08lld_%05d", prefix.c_str(), jobname.c_str(), context->jobID, partitionIdx);
		int fd = ::open((char*)fname.buf(), O_CREAT|O_RDWR, 0);
		if (-1 == fd) {
			perror("::open@MapOutput_impl::createBackupFile");
			exit(1);
		}
		return pair<int, string>(fd, (char*)fname.buf());
	}

	int createSocket(int iReduce, const vector<string>& reduceTaskIDs)
	{
		int fd = ::socket(AF_INET, SOCK_STREAM|SOCK_NONBLOCK, IPPROTO_TCP);
		if (-1 == fd) {
			perror("::socket@createSocket failed");
			exit(1);
		}
		return fd;
	}
};

#include "MapOutputNaive.h"

struct Buf
{
//	int  refcount;
//	int  bufsize;
	int  datalen; // may be less than bufsize
	int  nsent;

	unsigned char data[0];

	static Buf* create(int bufsize)
	{
		Buf* b = (Buf*)malloc(sizeof(Buf) + bufsize);
//		b->refcount = 0;
//		b->bufsize = bufsize;
		b->datalen = 0;
		b->nsent = 0;
		return b;
	}

	Buf* resize(int newBufsize)
	{
		assert(this);
		Buf* b = (Buf*)realloc(this, sizeof(Buf) + newBufsize);
//		b->bufsize = newBufsize;
		return b;
	}

	void sendsome(int sockfd)
	{
		assert(nsent < datalen);
		int some = ::write(sockfd, data + nsent, datalen - nsent);
		if (-1 == some) {
			if (EAGAIN != errno) {
				perror("::write@Buf::sendsome");
				exit(1);
			}
		}
	   	else
			nsent += some;
	}

	unsigned char* current() { return data + datalen; }

	bool completed() const { return (nsent == datalen); }
};

class BufMrPipe : public MrPipe
{
	std::queue<Buf*> bufqueue;
	boost::mutex bmutex;
	Buf* currbuf;
	Buf* sending;

public:
	string backupName;
	int  backupFd; ///< owned
	int  epollfd;  ///< not owned, only refer to it

	BufMrPipe()
	{
		currbuf = Buf::create(bufsize);
		sending = NULL;
	}

	~BufMrPipe()
	{
		assert(NULL == sending);
		assert(bufqueue.empty());
		assert(currbuf);
		::free(currbuf);
	}

	virtual void write(const void* key, size_t klen, const void* val, size_t vlen)
	{
		unsigned char klenbuf[5];
		ssize_t  klenlen = save_var_uint32(klenbuf, klen) - klenbuf;
		int32_t  recordlen = klenlen + klen + vlen;

		if (nrCurr >= nrFirst) {
			if (currbuf->datalen + 4 + recordlen > bufsize) {
				if (0 == currbuf->datalen) {
					// not enough to hold only one Record
					// enlarge currbuf
					bufsize = 4 + recordlen;
					currbuf = currbuf->resize(bufsize);
				}
				else {
					queueWrite();
					currbuf = Buf::create(bufsize);
				}
			}
		}
		MinMemIO mio((unsigned char*)currbuf->current());
		mio.write(&recordlen, 4		 );
		mio.write(klenbuf   , klenlen);
		mio.write(key       , klen   );
		mio.write(val       , vlen   );
		currbuf->datalen = mio.diff(currbuf->data);
		nrCurr++;
	}

	void flush()
	{
		if (currbuf && currbuf->datalen) {
			queueWrite();
		}
	}

	bool isClean() const
	{
		return nrCurr == nrFlushed && NULL == sending;
	}

	void sendsome()
	{
		for (;;) {
			if (NULL == sending) {
				{
					boost::mutex::scoped_lock lock(bmutex);
					if (bufqueue.empty()) {
						// no more buffers, delete sockfd from epollfd
						epoll_event ev;
						ev.events = EPOLLOUT;
						int ret = epoll_ctl(epollfd, EPOLL_CTL_DEL, sockfd, &ev);
						if (ret < 0) {
							perror("epoll_ctl:EPOLL_CTL_DEL");
							exit(1);
						}
						break;
					}
					else {
						sending = bufqueue.front();
						bufqueue.pop();
					}
				}
				assert(NULL != sending);
				// assume write disk file is more faster than socket
				//    but this is not always true! socket maybe faster than disk
				//    on this case, ::write here is a wrong choice
				//    a better choice maybe use aio, but aio on many disk files maybe still not enough fast
				//    maybe write all partitions data to single file a better choice
				// ::write maybe take a long time, must unlock `bmutex` before here..
				ssize_t nwritten = ::write(backupFd, sending->data, sending->datalen);
				if (nwritten != sending->datalen) {
					perror("::write@BufMrPipe::sendsome");
					exit(1);
				}
			}
			sending->sendsome(sockfd);
			if (sending->completed()) {
				::free(sending);
				sending = NULL;
				// now `sending` has successfully sent
				// but the `sockfd` maybe still writable, and bufqueue has more buffers
				// so try again, until it is not writable, or all the `bufqueue` has sent
			}
		   	else // sockfd has no more data to write
				break;
		}
	}

protected:
	void queueWrite()
	{
		boost::mutex::scoped_lock lock(bmutex);
		assert(currbuf);
		bufqueue.push(currbuf);
		epoll_event ev;
		ev.events = EPOLLOUT|EPOLLET;
		ev.data.ptr = this;
		if (-1 == epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfd, &ev)) {
			perror("epoll_ctl: EPOLL_ADD");
			exit(1);
		}
		currbuf = NULL;
	}
};

class MapOutput_impl_buf : public MapOutput_impl
{
	boost::thread* sendingthread;
	int  timeout; ///< in ms
	int  epollfd; ///< owned
	volatile bool running;

public:
	MapOutput_impl_buf(MapContext* context)
		: MapOutput_impl(context)
	{
		int maxEvents = nReduce;
		epollfd = epoll_create(maxEvents);
		if (-1 == epollfd) {
			perror("epoll_create");
			exit(1);
		}
		timeout = context->getll("MapOutput.send.timeout", 5000);
		const vector<string>& reduceTaskIDs = context->getMulti("reduce.TaskIDs");
		for (int i = 0; i < nReduce; ++i) {
			BufMrPipe* pipe = new BufMrPipe;
			pipe->sockfd = createSocket(i, reduceTaskIDs);
			pipe->bufsize = bufsize;
			pair<int, string> backup = createBackupFile(i, context);
			pipe->backupFd = backup.first;
			pipe->backupName = backup.second;
			partitions[i] = pipe;
		}
		running = true;
		sendingthread = new boost::thread(boost::bind(
					&MapOutput_impl_buf::sendingthread_func, this));
	}

	~MapOutput_impl_buf()
	{
		if (-1 != epollfd)
			::close(epollfd);

        // must has stopped by the caller
        assert(!running);

		delete sendingthread;
	}

	void wait()
	{
		running = false;
		sendingthread->join();
	}

private:
	void sendingthread_func()
	{
		vector<epoll_event> events(10);
		do {
			int nev = epoll_wait(epollfd, &events[0], events.size(), timeout);
			if (-1 == nev) {
				perror("epoll_wait@MapOutput_impl_buf::sendingthread_func");
				exit(1);
			}
			for (int i = 0; i < nev; ++i) {
				BufMrPipe* pipe = (BufMrPipe*)events[i].data.ptr;
				pipe->sendsome();
			}
		} while (running);
	}
};

//
///////////////////////////////////////////////////////////////////////////////////
//
MapOutput::MapOutput(MapContext* context)
{
	impl = NULL;
	const string implclass = context->get("MapOutput_impl", "MapOutput_impl_buf");
	if (implclass == "MapOutput_impl_buf") {
		impl = new MapOutput_impl_buf(context);
	}
	else {
		impl = new MapOutput_impl_naive(context);
	}
}

MapOutput::~MapOutput()
{
	assert(NULL != impl);
    try {
        impl->flush();
	    impl->wait();
    }
    catch (const std::exception& e) {
        fprintf(stderr, "MapOutput::~MapOutput(), exception.what=%s\n", e.what());
    }
    catch (...) {
        fprintf(stderr, "MapOutput::~MapOutput(), unknown exception\n");
    }
	delete impl;
}

void MapOutput::write(const void* key, size_t klen, const void* val, size_t vlen)
{
	impl->write(key, klen, val, vlen);
}

#if 0
//! caller must call flush & wait when completed
void MapOutput::wait()
{
	assert(NULL != impl);
    impl->flush();
	impl->wait();
}
#endif

