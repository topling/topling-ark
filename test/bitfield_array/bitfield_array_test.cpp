#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <terark/bitfield_array.hpp>

int main() {
	printf("GetAllSum<1,2,3>::value=%d\n", terark::detail::GetAllSum<1,2,3>::value);
	printf("PrefixSum<2,1,2,3>::value=%d\n", terark::detail::PrefixSum<2,1,2,3>::value);
	printf("GetNthArg<0,1,2,3>::value=%d\n", terark::detail::GetNthArg<0,1,2,3>::value);
	printf("GetNthArg<1,1,2,3>::value=%d\n", terark::detail::GetNthArg<1,1,2,3>::value);
	printf("GetNthArg<2,1,2,3>::value=%d\n", terark::detail::GetNthArg<2,1,2,3>::value);
	printf("sizeof(IF<2,char,long>::type)=%d\n", (int)sizeof(terark::detail::IF<2,char,long>::type));
	printf("sizeof(IF<size_t(-1),char,long>::type)=%d\n", (int)sizeof(terark::detail::IF<size_t(-1),char,long>::type));
	printf("sizeof(IF<size_t(-1)+1,char,long>::type)=%d\n", (int)sizeof(terark::detail::IF<size_t(-1)+1,char,long>::type));
	bitfield_array<12,16,32,63,64> bfa;
	bfa.resize(9);
	bfa.aset(7, 0xFCF, 0xCED6, 0xAAAABBCC, uint64_t(-1) >> 1, uint64_t(-1));
	bfa.set<0>(7, 0xFCF);
	bfa.set<1>(7, 0x1236);
	bfa.aset(8, 0xED3, 0xCED7, 0xAAAABBCD, (uint64_t(-1) >> 1)-1, -1);
	bfa.emplace_back(11, 12, 13, 14, -2);

	for (size_t i = 0; i < 7; ++i) {
		if (i % 2 == 0) {
			bfa.aset(i, 0xAAA, 0x5555, 0xAAAAAAAA, 0x5555555555555555, 0xAAAAAAAAAAAAAAAA);
		} else {
			bfa.aset(i, 0x555, 0xAAAA, 0x55555555, 0x7AAAAAAAAAAAAAAA, 0x5555555555555555);
		}
	}

	printf("bitfield.TotalBits=%d\n", bfa.TotalBits);
	for (size_t i = 0; i < bfa.size(); ++i) {
		printf("bfa[%zd]:BitPos%%64=%02zd : %03X %04X %08X %016lX %016lX\n"
			  , i, i*bfa.TotalBits%64
			  , bfa.get<0>(i)
			  , bfa.get<1>(i)
			  , bfa.get<2>(i)
			  , bfa.get<3>(i)
			  , bfa.get<4>(i)
			  );
	}

	bitfield_array<2> b2;
	for (size_t i = 0; i < 10000; ++i) {
		b2.emplace_back(i%2);
	}
	for (size_t i = 0; i < b2.size(); ++i) {
		assert(b2.get0(i) == i%2);
	}
	printf("b2.size() = %zd  b2.mem_size() = %zd\n", b2.size(), b2.mem_size());
	return 0;
}

