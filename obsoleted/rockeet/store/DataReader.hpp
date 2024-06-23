#ifndef __terark_rockeet_store_DataReader_h__
#define __terark_rockeet_store_DataReader_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
# pragma warning(disable: 4127)
#endif

#include <vector>
#include <boost/intrusive_ptr.hpp>

#include <terark/util/refcount.hpp>
#include <terark/util/DataBuffer.hpp>

namespace terark { namespace rockeet {

struct MultiReadCtrl
{
	union {
		off_t offset;  ///< as in param
		void* rawData; ///< as out param
	}u;
	size_t length;
};

class TERARK_DLL_EXPORT DataSet : public std::vector<MultiReadCtrl>
{
	DataBufferPtr m_buf;
public:
	virtual ~DataSet();
	void setBuf(DataBufferPtr bufp) { m_buf = bufp; }
	size_t totalSize() const;
};

class TERARK_DLL_EXPORT IDataReader : public RefCounter
{
public:
    virtual ~IDataReader();

	virtual void read(off_t offset, size_t size, void* pBuf) const = 0;

	/**
	 @param[inout] ds
	 */
	virtual void multiRead(DataSet& ds) const;

	/**
	 @brief create a IDataReader from 'fname' by policy
	 @param[in] policy
		available policies are:
			-# "aio", use aio_list_io
			-# "mem", load whole file into memory
			-# "mmap", use mmap
			-# "pread", use pread
	@note
	   all policies are thread safe
	 */
	static IDataReader* create(const std::string& policy, const std::string& fname);
};

} } // namespace terark::rockeet

#endif // __terark_rockeet_store_DataReader_h__
