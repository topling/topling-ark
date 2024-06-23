// valvec_sfinae.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

// 'initializing' : conversion from 'long' to 'char', possible loss of data
#pragma warning(disable:4244)

#include <terark/valvec.hpp>
#include <list>

using namespace terark;

int _tmain(int argc, _TCHAR* argv[])
{
	using namespace std;
	list<char> lc = { 'a', 'b', 'c' };
	list<long> ll = { 'a', 'b', 'c' };
    valvec<char> s1("abc", (char)3);
    valvec<char> s2('a', 'a');
	valvec<long> s3(lc.begin(), (char)3);
	valvec<char> s4(ll.begin(), (char)3); // should warn
	printf("s1=%.*s\n", s1.size(), s1.data());
	printf("s2=%.*s\n", s2.size(), s2.data());
	return 0;
}

