#ifndef __terark_rockeet_index_Index_h__
#define __terark_rockeet_index_Index_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
# pragma warning(disable: 4127)
#endif

#include <terark/util/refcount.hpp>
#include <terark/io/StreamBuffer.hpp>
#include <terark/c/trb.h>
#include <rockeet/index/Posting.hpp>
#include <rockeet/index/Term.hpp>

namespace terark { namespace rockeet {

class TERARK_DLL_EXPORT DeltaInvertIndex : public RefCounter
{
    trb_vtab m_vtab;
    trb_tree m_tree;
    uint64_t m_maxDocID;
	trb_func_data_get_size m_pfGetKeySize;
//	size_t (*m_pfGetKeySize)(const void* data);

private:
    DeltaInvertIndex();
	static size_t trbGetDataSize(const struct trb_vtab* vtab, const void* data);

public:
    static DeltaInvertIndex* builtInFixedKeyIndex(field_type_t type);

    static DeltaInvertIndex* fixedKeyIndex(int keySize, trb_func_compare compare);
    static DeltaInvertIndex* varKeyIndex(trb_func_compare compare, trb_func_data_get_size pfGetKeySize);

    virtual ~DeltaInvertIndex();

	void dio_load(PortableDataInput<InputBuffer>& input, unsigned version);
	void dio_save(PortableDataOutput<OutputBuffer>& output, unsigned version) const;

    void append(uint64_t docID, const TermInfo& ti);

    /**
     * keylen should determined by key's binary data
     */
    IPostingRange* fuzzyFind(const void* key);

	QueryResultPtr find(const void* key);

	field_type_t getKeyType() const { return m_vtab.key_type; }
};
typedef boost::intrusive_ptr<DeltaInvertIndex> DeltaInvertIndexPtr;

// class Document
// {
// public:
// 	DocTermVec  indexed;
// 	SmartBuffer stored;
// };

class TERARK_DLL_EXPORT Index : public RefCounter
{
	typedef std::map<std::string, DeltaInvertIndexPtr> fieldset_t;
	fieldset_t fields;

	uint64_t maxDocID;
	bool strictIndexType;

public:
	static Index* fromFile(const std::string& fname);
	void savetoFile(const std::string& fname) const;

	Index();

	void addField(const std::string& fieldname, DeltaInvertIndexPtr fieldindex);

	/**
	 @return docID
	 */
	uint64_t append(const DocTermVec& doc);

	void dio_save(PortableDataOutput<OutputBuffer>& obuf, unsigned version);
	void dio_load(PortableDataInput<InputBuffer>& ibuf, unsigned version);


	std::vector<uint64_t> search(const std::vector<std::pair<std::string, std::string> >& qf, const char* op);

private:
	void readIndexData(PortableDataInput<InputBuffer>& ibuf, unsigned version);
	QueryResultPtr searchTermImpl(const std::string& fieldname, const void* k, field_type_t t) const;

public:
// 	void addFieldIndex(const std::string& fieldname, DeltaInvertIndexPtr index)
// 	{
// 		fields[fieldname] = index;
// 	}

	template<class FieldType>
	QueryResultPtr searchTerm(const std::string& fieldname, const FieldType& k) const
	{
		return searchTermImpl(fieldname, &k, terark_get_field_type(&k));
	}
};
typedef boost::intrusive_ptr<Index> IndexPtr;

class TERARK_DLL_EXPORT IndexReader : public RefCounter
{
	std::map<std::string, DeltaInvertIndexPtr> m_fields;

public:
	virtual ~IndexReader();

	IndexReader(const std::string& dir);

    /**
     * keylen should determined by key's binary data
     */
    virtual IPostingRange* find(const void* key);
};


} } // namespace terark::rockeet

#endif // __terark_rockeet_index_Index_h__
