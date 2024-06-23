// virtual_func_ptr.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

struct A
{
	virtual void f1()
	{

	}
	virtual void f2() = 0;
	virtual void f3()
	{
		printf("A::f3\n");
	}
};
typedef void (A::*A_fp)();

struct B : public A
{
	void f1()
	{
		printf("B::f1\n");
	}
	void f2()
	{
		printf("B::f2\n");
	}
	void f3()
	{
		printf("B::f3\n");
	}
};

int main(int argc, char* argv[])
{
	A_fp pf1 = &A::f1;
	A_fp pf2 = &A::f2;
	A_fp pf3 = &A::f3;
	A* pa = new B;
	(pa->*pf1)();
	return 0;
}

