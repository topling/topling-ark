#ifndef __terark_rockeet_store_DocTable_h__
#define __terark_rockeet_store_DocTable_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
# pragma warning(disable: 4127)
#endif

#include <boost/intrusive_ptr.hpp>
#include <terark/util/refcount.hpp>
#include <rockeet/store/DataReader.hpp>

namespace terark { namespace rockeet {

class TERARK_DLL_EXPORT IDocTable : public RefCounter
{
protected:
    boost::intrusive_ptr<IDataReader> m_data;
public:
    virtual ~IDocTable();
    virtual size_t getData(uint64_t docID, DataBufferPtr& bufp) const = 0;
    virtual DataSet getMultiData(const std::vector<uint64_t>& docs) const = 0;

	void setDataReader(boost::intrusive_ptr<IDataReader> dr) { m_data = dr; }

	static IDocTable* createDocTableFixedSize(size_t docSize);
	static IDocTable* createDocTableVarSize32(const std::string& fname);
	static IDocTable* createDocTableVarSize64(const std::string& fname);
};
typedef boost::intrusive_ptr<IDataReader> IDocTablePtr;

} } // namespace terark::rockeet

#endif // __terark_rockeet_store_DocTable_h__
