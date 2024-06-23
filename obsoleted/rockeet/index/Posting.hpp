#ifndef __terark_rockeet_index_Posting_h__
#define __terark_rockeet_index_Posting_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
# pragma warning(disable: 4127)
#endif

#include <terark/util/refcount.hpp>
#include <terark/io/MemStream.hpp>
#include <rockeet/index/Hit.hpp>

namespace terark { namespace rockeet {

class TERARK_DLL_EXPORT IPostingRange : public RefCounter
{
protected:
    Hit* hit;
	size_t m_size;
public:
	size_t size() const { return m_size; }
    virtual bool empty() const = 0;
    virtual void pop_front() = 0; // similar with operator++
    Hit& deref() const { return *hit; }
};

class TERARK_DLL_EXPORT MemPostingRange : public IPostingRange
{
    PortableDataInput<SeekableMemIO> input;
public:
    MemPostingRange(void* data, size_t length, size_t count);
    bool empty() const;
    void pop_front();
};

//////////////////////////////////////////////////////////////////////////

class QueryResult : public RefCounter
{
protected:
	byte* plpos;
	byte* plend;

public:
	uint64_t docID;

	QueryResult() { plpos = plend = NULL; }

	virtual void initCursor() = 0;
	virtual void pop_front() = 0;

	virtual size_t size() const = 0;

	bool empty() const { return plpos < plend; }
};
typedef boost::intrusive_ptr<QueryResult> QueryResultPtr;

class TempQueryResult : public QueryResult
{
	std::vector<uint64_t> pl; // posting list
public:
	virtual void initCursor();
	virtual void pop_front();
	virtual size_t size() const;

	void put(uint64_t docID) { pl.push_back(docID); }
};

class TermQueryResult : public QueryResult
{
	byte *plbeg;
	size_t m_size;
public:
	TermQueryResult(byte* plbeg, byte* plend, size_t size);
	virtual void initCursor();
	virtual void pop_front();
	virtual size_t size() const;
};

} } // namespace terark::rockeet

#endif // __terark_rockeet_index_Posting_h__
