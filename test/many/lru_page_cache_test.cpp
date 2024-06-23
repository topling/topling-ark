#include <terark/zbs/lru_page_cache.hpp>
#include <terark/util/linebuf.hpp>
#include <terark/util/profiling.hpp>
#include <terark/io/FileStream.hpp>
#include <boost/intrusive_ptr.hpp>
#include <stdio.h>
#include <string>
//#include <map>
//#include <vector>
#include <thread>
#include <random>
#include <terark/hash_strmap.hpp>
#if defined(_MSC_VER)
	#include <io.h>
#else
	#include <sys/unistd.h>
#endif
#include <getopt.h>

using namespace terark;

intptr_t GetFD(int fd) {
	return TERARK_IF_MSVC(_get_osfhandle,)(fd);
}
void usage(const char* prog) {
	fprintf(stderr,
R"(usage: %s [-c] CacheSize MaxLoop MaxReadLen Threads Shards MaxFiles"
  options:
    -a aio
    -c check correctness
    -s benchmark os pread
    MaxFiles if sufficient, each cache op is time O(1)
)"
		, prog);
	exit(0);
}
struct MyFileStream : public FileStream {
	MyFileStream(fstring fname, fstring mode) : FileStream(fname, mode) {
		m_file_fd = GetFD(fileno(m_fp));
		m_file_size = fsize();
	}
	size_t m_file_size;
	intptr_t m_file_fd;
	intptr_t m_fi;
};
int main(int argc, char* argv[]) {
	bool check_correct = false;
	bool sys_pread_only = false;
	bool aio = false;
	for (int opt = 0; (opt = getopt(argc, argv, "asc")) != -1; ) {
		switch (opt) {
		case '?': usage(argv[0]);        break;
		case 'a': aio            = true; break;
		case 'c': check_correct  = true; break;
		case 's': sys_pread_only = true; break;
		}
	}
	if (argc < optind + 6) {
		usage(argv[0]);
		return 1;
	}
	const size_t  cachesize = terark::ParseSizeXiB(argv[optind + 0]);
	const size_t  maxloop   = terark::ParseSizeXiB(argv[optind + 1]);
	const size_t  maxread   = terark::ParseSizeXiB(argv[optind + 2]);
	const size_t  n_threads = strtoull(argv[optind + 3], NULL, 10);
	const size_t  shards    = strtoull(argv[optind + 4], NULL, 10);
	const size_t  maxFiles  = strtoull(argv[optind + 5], NULL, 10);
	printf("cachesize = %zd\n", cachesize);
	printf("maxloop   = %zd\n", maxloop);
	printf("maxread   = %zd\n", maxread);
	printf("n_threads = %zd\n", n_threads);
	printf("shards    = %zd\n", shards);
	printf("maxFiles  = %zd\n", maxFiles);
	hash_strmap<boost::intrusive_ptr<MyFileStream> > files;
	boost::intrusive_ptr<LruReadonlyCache>
		cache(LruReadonlyCache::create(cachesize, shards, maxFiles, aio));
	size_t filelen_sum = 0;
	{
		LineBuf fname;
		while (fname.getline(stdin) > 0) {
			fname.trim();
			try {
				auto fp = new MyFileStream(fname, "rb");
				fp->m_fi = cache->open(fp->m_file_fd);
				filelen_sum += fp->m_file_size;
				files[fname].reset(fp);
			} catch (const std::exception& ex) {
				printf("failed: fopen(%s) = %s\n", fname.p, ex.what());
			}
		}
	}
	printf("file num = %zd, file len sum = %zd\n", files.size(), filelen_sum);
	std::vector<size_t>  bytesRead(n_threads, 0);
	std::vector<std::thread> threads;
	threads.reserve(n_threads);
	auto thread_func = [&](size_t nth_thread) {
		valvec<byte_t> buf1;
		valvec<byte_t> buf2;
		std::mt19937_64 rnd(nth_thread);
		size_t curloop = 0;
		for (; curloop < maxloop; ++curloop) {
			size_t idx = rnd() % files.end_i();
			if (files.is_deleted(idx))
				continue;
			MyFileStream* f = files.val(idx).get();
			size_t fsize = f->m_file_size;
			if (0 == fsize)
				continue;
			auto fname = files.key_c_str(idx);
			size_t offset = rnd() % fsize;
			size_t length = rnd() % std::min(fsize - offset, maxread);
			if (sys_pread_only) {
				buf1.resize_no_init(length);
				intptr_t fd = f->m_file_fd;
				fdpread(fd, buf1.data(), length, offset);
			}
			else {
				LruReadonlyCache::Buffer b(&buf2);
				auto data = cache->pread(f->m_fi, offset, length, &b);
				if (buf2.data() == data) {
					assert(buf2.size() == length);
				}
				if (check_correct) {
					buf1.resize_no_init(length);
					intptr_t fd = f->m_file_fd;
					fdpread(fd, buf1.data(), length, offset);
					fstring d1 = buf1;
					fstring d2(data, length);
					size_t pos = d1.commonPrefixLen(d2);
					assert(pos == length);
					if (pos != length) {
						printf("offset = %zd, length = %zd, mismatch at pos: %zd, file: %s\n"
							, offset, length, pos, fname);
					}
				}
				if (threads.size() == 1 && rnd() % 1000 < 330) {
					intptr_t old_fi = f->m_fi;
					f->m_fi = cache->open(f->m_file_fd);
					cache->close(old_fi);
				}
			}
			bytesRead[nth_thread] += length;
		}
		printf("thread %2zd exit\n", nth_thread);
	};
	profiling pf;
	long long t0 = pf.now();
	for (size_t i = 0; i < threads.capacity(); ++i) {
		threads.emplace_back([&,i](){thread_func(i);});
	}
	for (auto& t : threads) {
		t.join();
	}
	long long t1 = pf.now();
	size_t totalread = 0;
	for (size_t x : bytesRead) totalread += x;
	printf("\n");
	printf("file num = %zd, file len sum = %zd\n", files.size(), filelen_sum);
	printf("time = %f sec, ops = %f M op/sec, throughput = %f MB/sec\n"
		, pf.sf(t0,t1)
		, n_threads*maxloop/pf.uf(t0,t1)
		, totalread/pf.uf(t0,t1)
	);
	printf("\n");
	cache->print_stat_cnt(stdout);
	return 0;
}
