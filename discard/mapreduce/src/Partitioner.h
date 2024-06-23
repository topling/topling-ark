#ifndef __mapreduce_Partition_h__
#define __mapreduce_Partition_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
# pragma warning(disable: 4127)
#endif

#include "config.hpp"

class MAP_REDUCE_DLL_EXPORT Partitioner
{
public:
	virtual ~Partitioner();

	virtual int hash(const void* key, int klen, int npart);
};

#endif //__mapreduce_Partition_h__

