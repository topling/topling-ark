#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <terark/util/profiling.hpp>

#if 0
	#define popcount __builtin_popcountll
	typedef long long pcblock;
#else
	#define popcount __builtin_popcount
	typedef unsigned pcblock;
#endif

long popcount_intrinsic(const pcblock* la, long len, long loop)
{
	long pc = 0;
	for (long j = 0; j < loop; ++j) {
		for (long i = 0; i < len; ++i) {
			pc += popcount(la[i]);
		}
	}
	return pc;
}

long popcount_intrinsic_unroll4(const pcblock* la, long len, long loop)
{
    long pc = 0;
    long unroll = len / 4;
    for (long j = 0; j < loop; ++j) {
        for (long i = 0; i < unroll; i += 4) {
            pc += popcount(la[i+0]);
            pc += popcount(la[i+1]);
            pc += popcount(la[i+2]);
            pc += popcount(la[i+3]);
        }
    }
    return pc;
}

long popcount_intrinsic_unroll8(const pcblock* la, long len, long loop)
{
    long pc = 0;
    long unroll = len / 8;
    for (long j = 0; j < loop; ++j) {
        for (long i = 0; i < unroll; i += 8) {
            pc += popcount(la[i+0]);
            pc += popcount(la[i+1]);
            pc += popcount(la[i+2]);
            pc += popcount(la[i+3]);
            pc += popcount(la[i+4]);
            pc += popcount(la[i+5]);
            pc += popcount(la[i+6]);
            pc += popcount(la[i+7]);
        }
    }
    return pc;
}

static const char bits_in[256] = {      // # of bits set in one char
	0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,
	1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
	1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
	2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
	1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
	2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
	2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
	3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
	1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
	2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
	2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
	3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
	2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
	3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
	3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
	4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8,
};
long popcount_array(const pcblock* la, long len, long loop)
{
	long pc = 0;
	for (long j = 0; j < loop; ++j) {
		const unsigned char* ba = (const unsigned char*)la;
		for (long i = 0; i < len * sizeof(pcblock); ++i) {
			pc += bits_in[ba[i]];
		}
	}
	return pc;
}

long popcount_array_unroll(const pcblock* la, long len, long loop)
{
	long pc = 0;
	for (long j = 0; j < loop; ++j) {
		const unsigned char* ba = (const unsigned char*)la;
		for (long i = 0; i < len * sizeof(pcblock); i += sizeof(pcblock)) {
			pc += bits_in[ba[i+0]];
			pc += bits_in[ba[i+1]];
			pc += bits_in[ba[i+2]];
			pc += bits_in[ba[i+3]];
			if (sizeof(pcblock) == 8) {
				pc += bits_in[ba[i+4]];
				pc += bits_in[ba[i+5]];
				pc += bits_in[ba[i+6]];
				pc += bits_in[ba[i+7]];
			}
		}
	}
	return pc;
}


int main(int argc, char* argv[])
{
	if (argc < 3) {
		fprintf(stderr, "usage len loop\n");
		exit(1);
	}
	long len  = strtol(argv[1], NULL, 10);
	long loop = strtol(argv[2], NULL, 10);
	using terark::profiling;
	profiling pf;
	pcblock* la = (pcblock*)malloc(sizeof(pcblock) * len);
	long long t0 = pf.now();
	long pc0 = popcount_intrinsic(la, len, loop);
	long long t1 = pf.now();
	long pc1 = popcount_array(la, len, loop);
	long long t2 = pf.now();
	long pc2 = popcount_array_unroll(la, len, loop);
	long long t3 = pf.now();
	long long t4 = pf.now();

	if (pc0 != pc1 || pc0 != pc2) {
		fprintf(stderr, "pc0=%ld, pc1=%ld, pc2=%ld\n", pc0, pc1, pc2);
	}

	printf("time[intrinsic=[total=%lld, avg=%f], array=[plain=[total=%lld, avg=%f], unroll=[total=%lld, avg=%f] intrin/array=%f]\n"
			, pf.ns(t0,t1), (double)pf.ns(t0,t1)/len/loop
			, pf.ns(t1,t2), (double)pf.ns(t1,t2)/len/loop
			, pf.ns(t2,t3), (double)pf.ns(t2,t3)/len/loop
			, (double)pf.ns(t0,t1)/pf.ns(t1,t2)
			);

	free(la);
	return 0;
}

