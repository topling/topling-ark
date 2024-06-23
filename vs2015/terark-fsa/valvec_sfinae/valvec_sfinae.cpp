// valvec_sfinae.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

// 'initializing' : conversion from 'long' to 'char', possible loss of data
#pragma warning(disable:4244)

#include <terark/valvec.hpp>
#include <terark/fsa/fsa.hpp>
#include <list>

using namespace terark;

int _tmain(int argc, _TCHAR* argv[])
{
	using namespace std;
	using namespace terark;
	printf("CharTarget<size_t>::StateBits=%d\n", CharTarget<size_t>::StateBits);
	printf("CharTarget<uint64_t>::StateBits=%d\n", CharTarget<uint64_t>::StateBits);
	printf("CharTarget<uint32_t>::StateBits=%d\n", CharTarget<uint32_t>::StateBits);
	list<char> lc = { 'a', 'b', 'c' };
	list<long> ll = { 'a', 'b', 'c' };
    valvec<char> s1("abc", (char)3);
    valvec<char> s2('a', 'a');
	valvec<long> s3(lc.begin(), (char)3);
	valvec<char> s4(ll.begin(), (char)3); // should warn
	printf("s1=%.*s\n", (int)s1.size(), s1.data());
	printf("s2=%.*s\n", (int)s2.size(), s2.data());
	return 0;
}

