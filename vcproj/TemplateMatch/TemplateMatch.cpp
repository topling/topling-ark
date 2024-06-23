// TemplateMatch.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include <stdio.h>
#include <string>

struct A1
{
	template<class Ch, class Tr, class Al>
	void operator<<(std::basic_string<Ch, Tr, Al>& x)
	{
		printf("void operator<<(std::basic_string<Ch, Tr, Al>& x)\n");
	}
};

struct A2
{
	void operator<<(std::string& x)
	{
		printf("void operator<<(std::string& x)\n");
	}
};

struct B1 : A1
{
	using A1::operator<<;

	template<class T>
	void operator<<(T& x)
	{
		printf("void operator<<(T& x)\n");
	}
};
struct B2 : A2
{
	using A2::operator<<;

	template<class T>
	void operator<<(T& x)
	{
		printf("void operator<<(T& x)\n");
	}
};


int main(int argc, char* argv[])
{
	std::string s = "abc";

	B1 b1;

	// 下面两行调用了不同的函数，为什么？
	// these two line call different function, why?
	b1 << s;
	b1.operator<<(s);

	printf("\n");

	B2 b2;

	// OK, 完全正确
	// OK, all right
	b2 << s;
	b2.operator<<(s);

	return 0;
}

