#ifndef __mapreduce_Reducer_h__
#define __mapreduce_Reducer_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
# pragma warning(disable: 4127)
#endif

#include "ReduceContext.h"
#include <terark/io/IStream.hpp>

class Reducer_impl;
class FileSystem;
class MAP_REDUCE_DLL_EXPORT Reducer
{
	Reducer_impl* impl;

public:
	Reducer(ReduceContext* context);
	virtual ~Reducer();

	terark::IOutputStream* createOutput(const char* subdir);

	virtual void reduce(const void* key, size_t klen, const void* val, size_t vlen) = 0;
	virtual void run();
};

#endif //__mapreduce_Reducer_h__

