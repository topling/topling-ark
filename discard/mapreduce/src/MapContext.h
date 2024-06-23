#ifndef __mapreduce_MapContext_h__
#define __mapreduce_MapContext_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
# pragma warning(disable: 4127)
#endif

#include "JobConfig.h"

class MAP_REDUCE_DLL_EXPORT MapContext : public JobConfig
{
public:
//	const std::string backupFileName();
	long long jobID;

	MapContext(int argc, char* argv[]);
};

#endif //__mapreduce_MapContext_h__

