#include "DocTable.hpp"
#include <terark/io/FileStream.hpp>

namespace terark { namespace rockeet {

IDocTable::~IDocTable()
{

}

//////////////////////////////////////////////////////////////////////////

class TERARK_DLL_EXPORT FixedDocTable : public IDocTable
{
    size_t m_size;
public:
	explicit FixedDocTable(size_t docSize) : m_size(docSize) {}
    virtual size_t getData(uint64_t docID, DataBufferPtr& bufp) const
	{
		if (bufp->size() < m_size)
			bufp = DataBufferPtr(m_size);
        m_data->read(docID*m_size, m_size, bufp->data());
		return m_size;
	}
    virtual DataSet getMultiData(const std::vector<uint64_t>& docs) const
	{
        DataSet ds; ds.resize(docs.size());
        for (ptrdiff_t i = 0; i != docs.size(); ++i)
        {
            uint64_t docID = docs[i];
            MultiReadCtrl& pos = ds[i];
            pos.u.offset = docID * m_size;
            pos.length   = m_size;
        }
        m_data->multiRead(ds);
		return ds;
	}
};
//////////////////////////////////////////////////////////////////////////

template<class OffsetType>
class TERARK_DLL_EXPORT VarDocTable : public IDocTable
{
    std::vector<OffsetType> m_index;

public:
	VarDocTable(const std::string& fname)
	{
		FileStream fp(fname.c_str(), "rb");
		stream_position_t length = fp.size();
		assert(length % sizeof(OffsetType) == 0);
		m_index.resize(length / sizeof(OffsetType));
		stream_position_t nRead = fp.read(&m_index[0], length);
		if (nRead != length)
		{
			throw IOException("MemDataReader::MemDataReader");
		}
	}

    virtual size_t getData(uint64_t docID, DataBufferPtr& bufp) const
    {
        assert(docID+1 < m_index.size());
        OffsetType offset = m_index[docID];
        size_t     length = m_index[docID+1] - offset;
		if (bufp->size() < length)
			bufp = DataBufferPtr(length);
        m_data->read(offset, length, bufp->data());
		return length;
    }
    virtual DataSet getMultiData(const std::vector<uint64_t>& docs) const
    {
        DataSet ds; ds.resize(docs.size());
        for (ptrdiff_t i = 0; i != docs.size(); ++i)
        {
            uint64_t docID = docs[i];
            MultiReadCtrl& pos = ds[i];
            pos.u.offset = m_index[docID];
            pos.length   = m_index[docID+1] - m_index[docID];
        }
		m_data->multiRead(ds);
        return ds;
    }
};

// template VarDocTable<uint32_t>;
// template VarDocTable<uint64_t>;

//////////////////////////////////////////////////////////////////////////
IDocTable* IDocTable::createDocTableFixedSize(size_t docSize)
{
	return new FixedDocTable(docSize);
}

IDocTable* IDocTable::createDocTableVarSize32(const std::string& fname)
{
	return new VarDocTable<uint32_t>(fname);
}

IDocTable* IDocTable::createDocTableVarSize64(const std::string& fname)
{
	return new VarDocTable<uint64_t>(fname);
}

} } // namespace terark::rockeet
