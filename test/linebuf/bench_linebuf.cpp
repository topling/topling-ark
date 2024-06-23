#define _CRT_SECURE_NO_WARNINGS
#include <terark/util/linebuf.hpp>
#include <terark/util/profiling.hpp>

int main(int argc, char* argv[]) {
	terark::profiling pf;
	terark::LineBuf lb;
	size_t bytes = 0;
	size_t lines = 0;
	long long t0 = pf.now();
	while (lb.getline(stdin) >= 0) {
		bytes += lb.size();
		lines += 1;
		lb.chomp();
	}
	long long t1 = pf.now();
	printf("time      = %8.3f sec\n", pf.sf(t0, t1));
	printf("bytes/sec = %8.3f M\n", bytes/pf.uf(t0, t1));
	printf("lines/sec = %8.3f M\n", lines/pf.uf(t0, t1));
	return 0;
}

