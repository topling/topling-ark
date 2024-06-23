#ifndef __mapreduce_ReduceContext_h__
#define __mapreduce_ReduceContext_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
# pragma warning(disable: 4127)
#endif

#include <terark/io/IStream.hpp>
#include "JobConfig.h"


class MAP_REDUCE_DLL_EXPORT ReduceContext : public JobConfig
{
public:
	ReduceContext(int argc, char* argv[]);
};

#endif //__mapreduce_ReduceContext_h__

