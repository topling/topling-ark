#pragma once

#include <boost/noncopyable.hpp>
#include <boost/intrusive_ptr.hpp>
#include <stdio.h>
#include <terark/config.hpp>
#include <terark/fstring.hpp>
#include <terark/valvec.hpp>

namespace terark { namespace zsrch {

class TERARK_DLL_EXPORT Query : boost::noncopyable {
	long m_refcnt;
friend void intrusive_ptr_add_ref(Query* p) { p->m_refcnt++; }
friend void intrusive_ptr_release(Query* p) { if (--p->m_refcnt) delete p; }
public:
	Query() { m_refcnt = 0; }
	static Query* createQuery(fstring strQuery);
	virtual ~Query();
};
typedef boost::intrusive_ptr<Query> QueryPtr;

struct DocMeta {
	size_t docID;
	size_t lineno;
	size_t fileIndex;
	std::string host;
	std::string path;
};

class TERARK_DLL_EXPORT IndexReader : boost::noncopyable {
	long m_refcnt;
friend void intrusive_ptr_add_ref(IndexReader* p) { p->m_refcnt++; }
friend void intrusive_ptr_release(IndexReader* p) { if (--p->m_refcnt) delete p; }
public:
	static IndexReader* loadIndex(const std::string& indexDir);
	IndexReader() { m_refcnt = 0; }
	void getDocList(const Query*, valvec<uint32_t>* docList) const;

	virtual ~IndexReader();
	virtual void getDocMeta(size_t docID, DocMeta* docMeta) const = 0;
	virtual void getDocData(size_t docID, std::string* data) const = 0;
	virtual void getTermList(const Query*, valvec<uint32_t>* termList) const = 0;
	virtual void getTermString(size_t termID, std::string* termStr) const = 0;
	virtual void getDocList(const valvec<uint32_t>& termList, valvec<uint32_t>* docList) const = 0;
	virtual void getDocList(size_t termID, valvec<uint32_t>* docList) const = 0;
	virtual size_t getDocListLen(size_t termID) const = 0;
};
typedef boost::intrusive_ptr<IndexReader> IndexReaderPtr;

}} // namespace terark::zsrch
