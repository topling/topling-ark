#ifndef __mapreduce_MapOutput_h__
#define __mapreduce_MapOutput_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
# pragma warning(disable: 4127)
#endif


#include "MapContext.h"

class MapOutput_impl;

class MAP_REDUCE_DLL_EXPORT MapOutput
{
public:
	MapOutput(MapContext* context);
	~MapOutput();

	void write(const void* key, size_t klen, const void* val, size_t vlen);

    /**
     * may not wait in destructor, because of:
     *  -# wait may throw exception
     *    - C++ does not allow throw exception in destructor
     *     - may cause undefined behavior
     * so wait here
     */
//	void wait();

protected:
	MapOutput_impl* impl;
};

#endif //__mapreduce_MapOutput_h__

