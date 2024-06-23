#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <terark/stdtypes.hpp>

inline int fast_popcount32(uint32_t v) {
	v = v - ((v >> 1) & 0x55555555);     // reuse input as temporary
	v = (v & 0x33333333) + ((v >> 2) & 0x33333333); // temp
	return (((v + (v >> 4)) & 0xF0F0F0F) * 0x1010101) >> 24;
}

inline int fast_popcount64(uint64_t v) {
	v = v - ((v >> 1) & 0x5555555555555555ull);
	v = (v & 0x3333333333333333ull) + ((v >> 2) & 0x3333333333333333ull);
	v = (v + (v >> 4)) & 0x0F0F0F0F0F0F0F0Full;
	return (v * 0x0101010101010101ull) >> 56;
}

int main(int argc, char* argv[]) {
	int loop = 1;
	if (argc >= 2) loop = atoi(argv[1]);
	for (int i = 0; i < loop; ++i) {
		uint32_t lo = rand();
		uint32_t hi = rand();
		uint32_t clo1 = __builtin_popcount(lo);
		uint32_t chi1 = __builtin_popcount(hi);
		uint32_t clh1 = __builtin_popcountll(uint64_t(hi) << 32 | lo);
		uint32_t clo2 = fast_popcount32(lo);
		uint32_t chi2 = fast_popcount32(hi);
		uint32_t clh2 = fast_popcount64(uint64_t(hi) << 32 | lo);
		TERARK_VERIFY_EQ(clo1, clo2);
		TERARK_VERIFY_EQ(chi1, chi2);
		TERARK_VERIFY_EQ(clh1, clh2);
		TERARK_VERIFY_EQ(clh1, clh2);
	}
	printf("All Passed\n");
	return 0;
}

