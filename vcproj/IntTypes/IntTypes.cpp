// IntTypes.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <stdlib.h>

// 编译环境 vs2008

#define GEN_FUNC(type) \
void foo(type x) \
{ \
	printf("sizeof(x)=%d, %s\n", sizeof(x), "void foo("#type" x)"); \
}

GEN_FUNC(int)
GEN_FUNC(unsigned __int64)
GEN_FUNC(unsigned int)
GEN_FUNC(__int64)

GEN_FUNC(unsigned long) // 如果注释这一行，foo(ul) 就编译不过，提示无法确定调用哪个函数

struct AAA
{
	int a;

	void foo(void* __restrict vp) __restrict
	{
		*(int*)vp = 1;
		a = 2;
	}
};

void restricted_foo(void* __restrict p1, void* __restrict p2)
{
	*(int*)p1 = *(int*)p2 + 3;
	*(int*)p2 = *(int*)p1 + 5;
}

inline void fa(AAA& x, int n)
{
	for (int i = 0; i < n; ++i)
	{
		x.a += x.a * (i+1) ^ i;
	}
}

int main(int argc, char* argv[])
{
	long l = 1000;
	unsigned long ul = 100;
	foo(l);  // 这一行始终可以编译过，调用的是 foo(int), long 的尺寸和int相等，因此不用提升到 __int64
	foo(ul); // 编译不过？unsigned long 和 unsigned int 的尺寸也是相等的呀

	int c = argc >= 2 ? atoi(argv[1]) : 1000;

	AAA x;
	fa(x, c);

	printf("x=%d\n", x.a);

	x.foo(&x);

	restricted_foo(&x, &x);

//	__if_exists(x)
//	__if_exists(AAA::a)
	{
		printf("");
	}

	return 0;
}

