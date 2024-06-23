#include <stdio.h>
//#include <x86intrin.h>
#include <immintrin.h>

int main(int argc, char* argv[]) {
	if (argc < 4) {
		fprintf(stderr, "usage: %s hexValue start length\n", argv[0]);
		return 1;
	}
	long long val = strtoull(argv[1], NULL, 16);
	long long beg = strtoull(argv[2], NULL, 10);
	long long len = strtoull(argv[3], NULL, 10);
	long long res = _bextr_u64(val, beg, len);
	printf("_bextr_u64(0x%016llX, %lld, %lld) = 0x%016llX\n", val, beg, len, res);
	return 0;
}
