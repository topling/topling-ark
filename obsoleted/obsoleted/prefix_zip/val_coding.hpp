/* vim: set tabstop=4 : */
#ifndef __terark_val_coding_h__
#define __terark_val_coding_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

#include <vector>

namespace terark { namespace prefix_zip {

template<class ValueT>
class ValueCodec
{
public:
	virtual size_t encode(const ValueT* p, size_t count, void* buf, size_t nBuf) = 0;

	virtual size_t val_count(const void* buf, size_t nBuf) = 0;
	virtual size_t decode(const void* buf, size_t nBuf , ValueT* p) = 0;

	virtual bool should_encode(const ValueT* p, size_t count) = 0;
};


} } // namespace terark::prefix_zip

#endif // __terark_val_coding_h__
