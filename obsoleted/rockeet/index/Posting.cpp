#include "Posting.hpp"

namespace terark { namespace rockeet {

	MemPostingRange::MemPostingRange(void* data, size_t length, size_t count)
	{
		m_size = count;
		input.set(data, length);
	}

	bool MemPostingRange::empty() const
	{
		return input.eof();
	}

	void MemPostingRange::pop_front()
    {
        var_uint64_t diff;
        input >> diff;
        hit->docID += diff;
        hit->loadvalue(input);
    }

//////////////////////////////////////////////////////////////////////////

	TermQueryResult::TermQueryResult(byte* plbeg, byte* plend, size_t size)
	{
		this->plbeg = plbeg;
		this->plpos = plbeg;
		this->plend = plend;
		this->m_size = size;
	}
	void TermQueryResult::pop_front()
	{
		assert(plpos < plend);

		uint64_t diff = load_var_uint64(plpos, (const byte**)&plpos);
		docID += diff;

		assert(plpos <= plend);
	}
	void TermQueryResult::initCursor()
	{
		plpos = plbeg;
		docID = 0;
		pop_front();
	}
	size_t TermQueryResult::size() const
	{
		return m_size;
	}

// 	TempQueryResult::TempQueryResult()
// 	{
//
// 	}

	void TempQueryResult::pop_front()
	{
		assert(plpos+8 <= plend);

		docID = *(uint64_t*)plpos;
		plpos += 8;
	}
	void TempQueryResult::initCursor()
	{
		if (pl.size()*8 < pl.capacity()*7)
			// free extra memory
			std::vector<uint64_t>(pl).swap(pl);

		plpos = (byte*)(&pl[0]);
		plend = (byte*)(&pl[0]+pl.size());
		docID = *(uint64_t*)plpos;
	}
	size_t TempQueryResult::size() const
	{
		return pl.size();
	}


} } // namespace terark::rockeet

