#ifndef __terark_rockeet_search_FuzzySearch_h__
#define __terark_rockeet_search_FuzzySearch_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
# pragma warning(disable: 4127)
#endif

#include <vector>
#include <boost/intrusive_ptr.hpp>
//
#include <terark/io/MemStream.hpp>
#include <terark/util/refcount.hpp>
// #include <terark/util/DataBuffer.hpp>
//
// #include <rockeet/doc/Schema.hpp>
#include <rockeet/store/DocTable.hpp>
#include <rockeet/index/Index.hpp>
#include <rockeet/search/Searcher.hpp>

namespace terark { namespace rockeet {


struct HitInfo
{
	const std::pair<std::string, DeltaInvertIndexPtr>* field;
	const void* pKey;
	const void* pVal;
	unsigned    cKey;
	unsigned    cVal;
	ptrdiff_t   iWay; //< way no
};

class HitInfoReader : public QueryResult
{
public:
	HitInfo hi;

	virtual void pop_front() = 0;
	virtual bool empty() = 0;
};
typedef boost::intrusive_ptr<HitInfoReader> HitInfoReaderPtr;

class HitExtractor : public RefCounter
{
public:
	virtual void mixAnd(uint64_t docID, HitInfo** hits, int nHits, AutoGrownMemIO& output);
	virtual void mixOr(uint64_t docID, HitInfo** hits, int nHits, AutoGrownMemIO& output);
	virtual void mixMinMatch(uint64_t docID, HitInfo** hits, int nHits, AutoGrownMemIO& output);
//	virtual uint64_t readDocID();
};
typedef boost::intrusive_ptr<HitExtractor> HitExtractorPtr;

class FuzzyQueryExpr : public RefCounter
{
public:
	HitExtractorPtr hitExtractor;
	HitInfoReaderPtr hitInfoReader;
	virtual QueryResultPtr eval();
};
typedef boost::intrusive_ptr<FuzzyQueryExpr> FuzzyQueryExprPtr;

class FuzzyOrExpr : public FuzzyQueryExpr
{
	std::vector<FuzzyQueryExprPtr> subs;
public:
	virtual void eval(QueryResult& result);
};

class FuzzyAndExpr : public FuzzyQueryExpr
{
	std::vector<FuzzyQueryExprPtr> subs;
public:
	virtual QueryResultPtr eval();
};

class FuzzyMinMatchExpr : public FuzzyQueryExpr
{
	// 在 subs.size() 中，最少有多少个相同的
	std::vector<FuzzyQueryExprPtr> subs;
	size_t minMatch;
public:
	virtual QueryResultPtr eval();
};

class FuzzySubstractExpr : public FuzzyQueryExpr
{
	FuzzyQueryExprPtr left;
	FuzzyQueryExprPtr right;
public:
	virtual QueryResultPtr eval();
};

class TERARK_DLL_EXPORT FuzzySearch : public RefCounter
{
	typedef std::map<std::string, DeltaInvertIndexPtr> fieldset_t;
	fieldset_t m_fields;

public:
	void addFieldIndex(const std::string& fieldname, DeltaInvertIndexPtr index)
	{
		m_fields[fieldname] = index;
	}

	std::vector<uint64_t> search(const std::string& strQuery);

//	std::vector<uint64_t> search(const std::vector<std::pair<std::string, std::string> >& qf, const char* op);
};

} } // namespace terark::rockeet

#endif // __terark_rockeet_search_FuzzySearch_h__
