#include "Reducer.h"
#include "dfs.h"
#include <terark/trb_cxx.hpp>
#include <terark/set_op.hpp>
#include <terark/io/var_int.hpp>

#include <netinet/in.h>
#include <sys/epoll.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#include <string.h>

using namespace terark;

//
// |<-------- nBuf -------->|
// |------------------------|
// | offset | nFill | free  |
// |------------------------|
struct ReduceInput
{
	unsigned char*	buf;
	ssize_t			nBuf;
	ssize_t			offset;
	ssize_t			nFill;
	ssize_t			currRecordNo;
	const unsigned char *key, *val;
	size_t			klen, vlen;
	time_t			expireTime;
	int				activeHeapIdx;
	int				sockfd;
	bool			softEof;
	bool			hardEof;
	sockaddr_storage peerAddr;
	std::string      peerName; // host:port

	static int comparePeerAddr(const trb_vtab* vtab, const trb_tree* tree, const void* x, const void* y)
	{
		return memcmp(x, y, sizeof(sockaddr_storage));
	}

	void init(size_t nBuf)
	{
		buf = (unsigned char*)malloc(nBuf);
		offset = 0;
		nFill  = 0;
		currRecordNo  = 0;
		sockfd = -1;
		softEof = false;
		hardEof = false;
		memset(&peerAddr, 0, sizeof(peerAddr));
	}

	~ReduceInput()
	{
		if (buf) ::free(buf);
		if (-1 != sockfd) ::close(sockfd);
	}

	void readRecord(size_t rlen)
	{
		unsigned char* p = buf + offset;
		klen = load_var_uint32(p, &key);
		assert(klen <= rlen);
		vlen = rlen - klen;
		val  = key + klen;
		nFill -= 4 + rlen;
		offset += 4 + rlen;
	}

	bool readSome()
	{
		assert(offset + nFill < nBuf);
		ssize_t toRead = nBuf - (offset + nFill);
		ssize_t  nRead = ::read(sockfd, buf + offset + nFill, toRead);
		if (-1 == nRead) {
			if (EWOULDBLOCK != errno) {
				perror("::read sockfd failed");
				exit(EXIT_FAILURE);
			}
		   	else
				return false;
		}
	   	else if (0 == nRead) {
			if (softEof) {
				assert(0 == nFill);
				hardEof = true;
				return false;
			}
		}
		nFill += nRead;
		if (nFill >= 4) {
			uint32_t rlen;
			memcpy(&rlen, buf + offset, 4);
			if (ssize_t(4 + rlen) > nBuf) {
				// buf too small, can not hold one record, increase
				buf = (unsigned char*)realloc(buf, 2 * nBuf);
				if (NULL == buf) {
					perror("readSome:realloc");
					abort();
				}
				return false;
			}
			if (ssize_t(4 + rlen) <= nFill) {
				// read first record this time
				readRecord(rlen);
				return true;
			}
			if (0 == rlen) {
				softEof = true;
			}
		}
		return false;
	}

	bool next()
	{
		if (nFill >= 4) {
			uint32_t rlen;
			memcpy(&rlen, buf + offset, 4);
			if (ssize_t(4 + rlen) <= nFill) {
				readRecord(rlen);
				return true;
			}
		}
		// move to front
		memmove(buf, buf + offset, nFill);
		offset = 0;
		return false;
	}

	void reduce(Reducer* reducer)
	{
		reducer->reduce(key, klen, val, vlen);
	}
};

struct CompExpireTime
{
	bool operator()(const ReduceInput* x, const ReduceInput* y) const
	{
		return x->expireTime > y->expireTime;
	}
};

struct UpdateHeapIndex
{
	void operator()(ReduceInput* x, int index) const
	{
		x->activeHeapIdx = index;
	}
};

class Reducer_impl
{
	typedef terark::trbtab
		< sockaddr_storage
		, ReduceInput
		, offsetof(ReduceInput, peerAddr)
		, &ReduceInput::comparePeerAddr
	   	> input_tab_t;
	input_tab_t inputs;
	std::vector<ReduceInput*> activeHeap; // for timeout manage
//	Reducer* reducer;
	int  listenFd;
	int  timeout;
	bool running;

	ReduceContext* context;
	FileSystem* fs;

public:
	Reducer_impl(ReduceContext* context)
		: context(context)
	{
		fs = context->getFileSystem();
	}

	~Reducer_impl()
	{
		delete fs;
	}

	terark::IOutputStream* createOutput(const char* subdir)
	{
		std::string outputDir = context->get("reduce.output.dir", "");
		if (outputDir.empty()) {
			char buf[128];
			int pos = snprintf(buf, sizeof(buf), "/MapReduce/output");
			struct tm ltm;
			time_t now = ::time(NULL);
			localtime_r(&now, &ltm);
			strftime(buf + pos, sizeof(buf) - pos, "%Y/%m/%d/%H/%M-%S", &ltm);
			outputDir = buf;
		}
		if (NULL != subdir && '\0' != subdir[0]) {
			outputDir += "/";
			outputDir += subdir;
		}
		outputDir += "/part-";
		outputDir += context->get("reduce.partition.nth", "0");

		terark::IOutputStream* os = fs->openOutput(outputDir.c_str(), O_CREAT|O_TRUNC);

		return os;
	}

	void run(Reducer* reducer, ReduceContext* context)
	{
		std::vector<epoll_event> events(1024);
		int epollfd = epoll_create(events.size());
		if (epollfd == -1) {
			perror("epoll_create");
			exit(EXIT_FAILURE);
		}
		epoll_event ev;
		ev.events   = EPOLLIN;
		ev.data.ptr = &listenFd;
		if (epoll_ctl(epollfd, EPOLL_CTL_ADD, listenFd, &ev) == -1) {
			perror("epoll_ctl:add:listen");
			exit(EXIT_FAILURE);
		}
		running = true;
		do {
			int nfds = epoll_wait(epollfd, &events[0], events.size(), timeout);
			if (nfds == -1) {
				perror("epoll_wait");
				exit(EXIT_FAILURE);
			}
			// currTime for timeout management, use the time at epoll_wait return
			time_t currTime = ::time(0);
			for (int i = 0; i < nfds; ++i) {
				if (&listenFd == events[i].data.ptr)
					acceptFd(epollfd);
				else
					handleInput(reducer, events[i]);
			}
			if (!activeHeap.empty()) {
				ReduceInput* earliest = activeHeap[0];
				if (earliest->expireTime < currTime) {
					// timeout, delete earliest
					pop_heap_ignore_top(activeHeap.begin(), activeHeap.end()
							, CompExpireTime(), UpdateHeapIndex());
					activeHeap.pop_back();
				//	inputs.erase(earliest->peerAddr);
				}
			}
		} while (running);

		::close(epollfd);
	}

private:
	void handleInput(Reducer* reducer, epoll_event ev)
	{
		// use ev.data.ptr
		ReduceInput* input = (ReduceInput*)ev.data.ptr;
		assert(input == activeHeap[input->activeHeapIdx]);
		if (ev.events & EPOLLERR) {
			delfromHeap(input);
		//	inputs.erase(input->peerAddr);
		}
		else {
			while (input->readSome()) {
				do {
					input->reduce(reducer);
				} while (input->next());
			}
			if (input->hardEof) {
				delfromHeap(input);
			//	inputs.erase(input->peerAddr);
			}
		   	else {
				// 使用堆，对每个不同的 input, timeout 可以不同，并且每次 expireTime 的增量也可以不同
				// 使用 linklist 做超时管理，timeout 必须自始至终不能变化
				// use heap, timeout can be different for any input, and at every adjust
				// use linklist, timeout must be fixed
				input->expireTime = ::time(0) + timeout;
				terark::adjust_heap_hole(activeHeap.begin(), activeHeap.end()
						, input->activeHeapIdx
						, CompExpireTime()
						, UpdateHeapIndex()
						);
				assert(input == activeHeap[input->activeHeapIdx]);
			}
		}
	}

	void delfromHeap(ReduceInput* input)
	{
		if (activeHeap.back() != input) {
			// delete `input` from activeHeap
			terark_adjust_heap(activeHeap.begin()
					, (ptrdiff_t)input->activeHeapIdx
					, (ptrdiff_t)activeHeap.size()-1
					, activeHeap.back()
					, CompExpireTime()
					, UpdateHeapIndex()
					);
		}
		activeHeap.pop_back();
	}

//	static
	void setnonblocking(int fd)
	{
		const int flags = fcntl(fd, F_GETFL, 0);
		if (-1 == flags) {
			perror("fcntl:F_GETFL@setnonblocking");
			exit(1);
		}
		if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
			perror("fcntl:F_SETFL@setnonblocking");
			exit(1);
		}
	}

	void acceptFd(int epollfd)
	{
		size_t buflen = 8 * 1024;

		trb_node* node = inputs.alloc_node();
		ReduceInput* input = inputs.get_data(node);
		socklen_t addrlen;
		int connFd = ::accept(listenFd, (sockaddr*)&input->peerAddr, &addrlen);
		if (-1 == connFd) {
			perror("accept");
			inputs.free_node(node);
			exit(EXIT_FAILURE);
		}
		setnonblocking(connFd);
		epoll_event ev;
		ev.events   = EPOLLIN|EPOLLET;
		ev.data.ptr = input;
		if (epoll_ctl(epollfd, EPOLL_CTL_ADD, connFd, &ev) == -1) {
			perror("epoll_ctl@Reducer_impl::acceptFd.EPOLL_CTL_ADD");
			inputs.free_node(node);
			exit(EXIT_FAILURE);
		}
		trb_node* node2 = inputs.probe_node(node);
		if (node2 != node) {
			inputs.free_node(node);
			fprintf(stderr, "inputs.probe_node return another node\n");
			exit(1);
		}
		input->init(buflen);
		input->expireTime = ::time(0) + timeout;
		activeHeap.push_back(input);
		terark_push_heap(activeHeap.begin()
				, (ptrdiff_t)activeHeap.size()-1
				, (ptrdiff_t)0
				, input
				, CompExpireTime()
				, UpdateHeapIndex()
				);
		assert(input == activeHeap[input->activeHeapIdx]);
	}
};

//
////////////////////////////////////////////////////////////////
//
Reducer::Reducer(ReduceContext* context)
{
}

Reducer::~Reducer()
{
}

void Reducer::run(ReduceContext* context)
{
	impl->run(this, context);
}

terark::IOutputStream* Reducer::createOutput(const char* subdir)
{
	return impl->createOutput(subdir);
}


