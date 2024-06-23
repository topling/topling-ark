// PragmaPackBitField.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <terark/fsa/dfa_algo.hpp>

using namespace terark;

#pragma pack(push,1)
struct A {
	unsigned long x : 23;
	unsigned char y : 1;
};
struct B {
	unsigned long x : 23;
	unsigned long y : 1;
};
struct C {
	unsigned long x : 23;
	unsigned long y : 17;
};
struct D {
	unsigned long  x : 31;
	unsigned short y :  9;
};
#pragma pack(pop)

template<class Base>
struct TwoPhase : Base {
	void f() {
		printf("abc\n");
	}
};
struct Hack {
	void printf(const char*) {
		::printf("Hacked!\n");
	}
};

int _tmain(int argc, _TCHAR* argv[])
{
	printf("__lzcnt16(%llX)=%d\n", 1ULL<<15, __lzcnt  (1ULL<<15));
	printf("__lzcnt32(%llX)=%d\n", 1ULL<<31, __lzcnt  (1ULL<<31));
//	printf("__lzcnt64(%llX)=%d\n", 1ULL<<63, __lzcnt64(1ULL<<63));
	printf("__lzcnt32(%0lX)=%d\n", -1L , __lzcnt  (-1L ));
//	printf("__lzcnt64(%llX)=%d\n", -1LL, __lzcnt64(-1LL));
	printf("sizeof(size_t)=%d\n", (int)sizeof(size_t));
	printf("sizeof(A)=%d\n", (int)sizeof(A));
	printf("sizeof(B)=%d\n", (int)sizeof(B));
	printf("sizeof(C)=%d\n", (int)sizeof(C));
	printf("sizeof(D)=%d\n", (int)sizeof(D));
	printf("StaticUintBits<11>::value=%d\n", StaticUintBits<11>::value);
	printf("StaticUintBits<15>::value=%d\n", StaticUintBits<15>::value);
	printf("StaticUintBits<31>::value=%d\n", StaticUintBits<31>::value);
	printf("StaticUintBits<63>::value=%d\n", StaticUintBits<63>::value);
	printf("StaticUintBits<127>::value=%d\n", StaticUintBits<127>::value);
	printf("StaticUintBits<255>::value=%d\n", StaticUintBits<255>::value);
	printf("StaticUintBits<256>::value=%d\n", StaticUintBits<256>::value);
	printf("StaticUintBits<511>::value=%d\n", StaticUintBits<511>::value);
	DWORD written = 0;
	if (!WriteFile((HANDLE)2, "WriteFile\r\n", 11, &written, NULL)) {
		DWORD err = GetLastError();
		fprintf(stderr, "WriteFile.ErrCode=%d(%X)\n", err, err);
	}
	TwoPhase<Hack>().f();
	return 0;
}

