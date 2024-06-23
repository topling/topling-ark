#ifndef __terark_rockeet_search_Searcher_h__
#define __terark_rockeet_search_Searcher_h__

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

namespace terark { namespace rockeet {

class TERARK_DLL_EXPORT QueryParser : public RefCounter
{
public:

};

class TERARK_DLL_EXPORT QueryExpression : public RefCounter
{
public:
	virtual QueryResultPtr eval() = 0;
};
typedef boost::intrusive_ptr<QueryExpression> QueryExpressionPtr;

class TERARK_DLL_EXPORT OrExpression : public QueryExpression
{
public:
	std::vector<QueryExpressionPtr> subs;
	virtual QueryResultPtr eval();
};

class TERARK_DLL_EXPORT AndExpression : public QueryExpression
{
public:
	std::vector<QueryExpressionPtr> subs;
	virtual QueryResultPtr eval() = 0;
};

class TERARK_DLL_EXPORT MinMatchExpression : public QueryExpression
{
	// 在 subs.size() 中，最少有多少个相同的
public:
	std::vector<QueryExpressionPtr> subs;
	size_t minMatch;

	virtual QueryResultPtr eval() = 0;
};

class TERARK_DLL_EXPORT SubtractExpression : public QueryExpression
{
public:
	QueryExpressionPtr left;
	QueryExpressionPtr right;

	virtual QueryResultPtr eval() = 0;
};

class TERARK_DLL_EXPORT XorExpression : public QueryExpression
{
public:
	QueryExpressionPtr left;
	QueryExpressionPtr right;
	virtual QueryResultPtr eval() = 0;
};

} } // namespace terark::rockeet

#endif // __terark_rockeet_search_Searcher_h__
