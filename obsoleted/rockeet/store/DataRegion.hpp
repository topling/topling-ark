#ifndef __terark_rockeet_store_DataReader_h__
#define __terark_rockeet_store_DataReader_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
# pragma warning(disable: 4127)
#endif

#include <terark/io/MemStream.hpp>

namespace terark { namespace rockeet {

class TERARK_DLL_EXPORT DataRegion : public SeekableMemIO
{
	long m_nRef;
	bool m_isOwner;

	void set(); // hide SeekableMemIO::set

	friend void intrusive_ptr_add_ref(DataRegion* p) { ++p->m_nRef; }
	friend void intrusive_ptr_release(DataRegion* p) { if (0 == --p->m_nRef) delete p; }

public:
    DataRegion(unsigned char* data, size_t size)
    {
		SeekableMemIO::set(data, size);
		m_nRef = 0;
		m_isOwner = false;
    }
    DataRegion(size_t size)
    {
		SeekableMemIO::set(malloc(size), size);
		m_nRef = 0;
		m_isOwner = true;
    }

    ~DataRegion()
    {
		if (m_isOwner && m_beg)
			free(m_beg);
    }
};
typedef boost::intrusive_ptr<DataRegion> DataRegionPtr;

} } // namespace terark::rockeet

#endif // __terark_rockeet_store_DataReader_h__
