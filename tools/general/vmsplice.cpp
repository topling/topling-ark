//
// Created by leipeng on 2021/5/18.
//
#include <getopt.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/uio.h> // vmsplice
#include <random>
#include <terark/fstring.hpp>
#include <terark/util/function.hpp>
#include <terark/util/stat.hpp>

using namespace terark;
void usage(const char* prog) {
	fprintf(stderr,
R"EOS(Usage: %s Options file_name
  Options:
    -w required size of write file with random data
    -h Show this help information
    -v Show verbose info
)EOS", prog);
	exit(1);
}

static void fill_rand(uint64_t* base, size_t fsize) {
    std::mt19937_64 rnd(fsize);
    for (size_t i = 0; i < fsize/4096;) {
        uint64_t rv = rnd();
        for (size_t j = 0; j < 4096/8; ++j, ++i) base[i] = rv;
    }
}

int main(int argc, char* argv[]) {
    size_t fsize = 0;
    bool use_vmsplice = false;
	for (;;) {
		int opt = getopt(argc, argv, "hmvw:");
		switch (opt) {
		case -1:
			goto GetoptDone;
		case 'm':
		    use_vmsplice = true;
		    break;
		case 'w':
            fsize = align_up((size_t)ParseSizeXiB(optarg), 4096);
            break;
		case 'v':
		//	verbose = true;
			break;
		case '?':
		case 'h':
		default:
			usage(argv[0]);
		}
	}
GetoptDone:
    if (0 == fsize || optind >= argc) {
        usage(argv[0]);
    }
    const char* fname = argv[optind];
    int fd = open(fname, O_CREAT|O_CLOEXEC|O_WRONLY, 0644);
    if (fd < 0) {
        fprintf(stderr,
                "ERR: open(%s, O_CREAT|O_CLOEXEC|O_WRONLY, 0644) = %s\n",
                fname, strerror(errno));
        return 1;
    }
    TERARK_SCOPE_EXIT(close(fd));
    auto mem = (uint64_t*)mmap(NULL, fsize, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE, -1, 0);
    if (MAP_FAILED == mem) {
        close(fd);
        fprintf(stderr, "ERR: mmap(len = %zd) = %s\n", fsize, strerror(errno));
        return 1;
    }
    TERARK_SCOPE_EXIT(munmap(mem, fsize));
    fill_rand(mem, fsize);
    if (use_vmsplice) {
        struct iovec io = { mem, fsize };
        auto n = vmsplice(fd, &io, 1, SPLICE_F_GIFT);
        if (size_t(n) != fsize) {
            fprintf(stderr, "ERR: vmsplice(%s, size=%zd) = %s\n",
                    fname, fsize, strerror(errno));
            return 1;
        }
    }
    else {
        auto n = write(fd, mem, fsize);
        if (size_t(n) != fsize) {
            fprintf(stderr, "ERR: write(%s, size=%zd) = %s\n",
                    fname, fsize, strerror(errno));
            return 1;
        }
    }
    return 0;
}
