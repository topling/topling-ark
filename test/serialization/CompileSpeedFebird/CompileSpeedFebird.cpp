
#include <stdio.h>
#include <terark/io/DataIO.hpp>
#include <terark/io/StreamBuffer.hpp>
#include <terark/io/FileStream.hpp>

using namespace boost;
using namespace terark;
using namespace std;

typedef unsigned char uchar;
typedef signed char schar;

typedef long long longlong;
typedef unsigned long long ulonglong;

#include "data_terark.h"

int main(int argc, char* argv[])
{
	terark_pod_0 bp0;
	terark_pod_1 bp1;
	terark_pod_2 bp2;
	terark_complex_0 bc0;
	terark_complex_1 bc1;
	terark_complex_2 bc2;

	{
		FileStream file("boost.bin", "w+");
		NativeDataOutput<OutputBuffer> ar;
		ar.attach(&file);
		ar & bp0;
		ar & bp1;
		ar & bp2;
		ar & bc0;
		ar & bc1;
		ar & bc2;
		terark_foo(ar);
	}

	{
		FileStream file("boost.bin", "r");
		NativeDataInput<InputBuffer> ar;
		ar.attach(&file);
		ar & bp0;
		ar & bp1;
		ar & bp2;
		ar & bc0;
		ar & bc1;
		ar & bc2;
		terark_foo(ar);
	}
	return 0;
}

