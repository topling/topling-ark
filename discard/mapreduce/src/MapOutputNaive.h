
class MemBackMrPipe : public MrPipe
{
	AutoGrownMemIO mem;
	size_t offsetFlushed;

public:
	MemBackMrPipe()
	{
		offsetFlushed = 0;
	}

	virtual void write(const void* key, size_t klen, const void* val, size_t vlen)
	{
		unsigned char klenbuf[5];
		ssize_t  klenlen = save_var_uint32(klenbuf, klen) - klenbuf;
		int32_t  recordlen = klenlen + klen + vlen;

		if (nrCurr == nrFirst) {
			assert(0 == offsetFlushed);
			offsetFlushed = mem.tell();
		}
		mem.write(&recordlen, 4      );
		mem.write(klenbuf   , klenlen);
		mem.write(key       , klen   );
		mem.write(val       , vlen   );

		if (nrCurr >= nrFirst && int(mem.tell() - offsetFlushed) >= bufsize) {
			flush();
		}
		nrCurr++;
	}

	void flush()
	{
		ssize_t len = mem.tell() - offsetFlushed;
		ssize_t nwritten = ::write(sockfd, mem.begin() + offsetFlushed, len);
		if (nwritten != len) {
			// should be fault tolerant
			perror("::write sockfd failed");
			exit(1);
		}
		offsetFlushed = mem.tell();
		nrFlushed     = nrCurr;
	}
};

class FileBackMrPipe : public MrPipe
{
	AutoGrownMemIO mem;
	int   firstOffset; ///< 第一条需要发送的记录在 mem 中的偏移

public:
	int   backupFd;
	string backupName;

	FileBackMrPipe(int bufsize)
	{
		mem.resize(bufsize);
		this->bufsize = bufsize;
		firstOffset = 0;
	}

	~FileBackMrPipe()
	{
		if (-1 != backupFd)
			::close(backupFd);
//		::remove(backupName.c_str());
	}

	virtual void write(const void* key, size_t klen, const void* val, size_t vlen)
	{
		unsigned char klenbuf[5];
		ssize_t  klenlen  = save_var_uint32(klenbuf, klen) - klenbuf;
		int32_t  recordlen = klenlen + klen + vlen;
		ssize_t  total    = mem.tell() + 4 + recordlen;

		if (nrCurr == nrFirst)
			firstOffset = mem.tell();

		if (total > (ssize_t)mem.size()) {
			struct iovec iov[5] =
			{
				{ mem.begin(), mem.tell() },
				{ &recordlen , 4          },
				{ klenbuf    , klenlen    },
				{ (void*)key , klen       },
				{ (void*)val , vlen       }
			};
			// 先备份，这种实现方式可能导致大量随机磁盘IO，这里先不解决这个问题
			//                             ^^^^^^^^^^^^^^      ^^^^^^^^
			ssize_t nwritten = ::writev(backupFd, iov, 5);
			if (nwritten != (ssize_t)total) {
				perror("writev to backupFd failed");
				exit(1);
			}
			if (nrCurr >= nrFirst) {
				if (0 != firstOffset) {
					// 这是第一次发送记录，并且有需要略过的记录
					iov[0].iov_base  = (char*)iov[0].iov_base + firstOffset;
					iov[0].iov_len  -= firstOffset;
					firstOffset = 0;
				}
				nwritten = ::writev(sockfd, iov, 5);
				if (nwritten != total) {
					// should be fault tolerant
					perror("writev to sockfd failed");
					exit(1);
				}
			}
			nrFlushed = nrCurr;
			mem.rewind();
		}
		else {
			mem.write(&recordlen, 4      );
			mem.write(klenbuf   , klenlen);
			mem.write(key       , klen   );
			mem.write(val       , vlen   );
		}
		nrCurr++;
	}

	void flush()
	{
		ssize_t nwritten;
		nwritten = ::write(backupFd, mem.begin(), mem.tell());
		if (nwritten != (ssize_t)mem.tell()) {
			perror("writev@flush to backupFd failed");
			exit(1);
		}
		nwritten = ::write(sockfd  , mem.begin(), mem.tell());
		if (nwritten != (ssize_t)mem.tell()) {
			perror("writev@flush to sockfd failed");
			exit(1);
		}
	}
};

class MapOutput_impl_naive : public MapOutput_impl
{
public:
	MapOutput_impl_naive(MapContext* context)
		: MapOutput_impl(context)
	{
		const string backupType = context->get("MapOutput.backupType", "mem");
		if (backupType == "mem") {
			for (int i = 0; i != nReduce; ++i) {
				MemBackMrPipe* pipe = new MemBackMrPipe;
				partitions[i] = pipe;
			}
		}
		else if (backupType == "file") {
			for (int i = 0; i != nReduce; ++i) {
				FileBackMrPipe* pipe = new FileBackMrPipe(bufsize);
				pair<int, string> backup = createBackupFile(i, context);
				pipe->backupFd = backup.first;
				pipe->backupName = backup.second;
				partitions[i] = pipe;
			}
		}
		else {
			fprintf(stderr, "invalid MapOutput.backupType=%s\n", backupType.c_str());
			::exit(1);
		}
	}

	void wait()
	{
	}
};


