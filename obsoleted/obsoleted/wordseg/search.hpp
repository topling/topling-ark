/* vim: set tabstop=4 : */
#ifndef __terark_wordseg_search_h__
#define __terark_wordseg_search_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

#include "compare.hpp"

namespace terark { namespace wordseg {

template< class CharT
		, class GetCharElem = GetCharFromBegin // GetCharFromEnd
		, class GetCharCode = XIdentity // char_tolower
		, class GetStringEl = XIdentity // get some member
		, class CompareCharCode = CharLess // CharGreater
		>
class StrCharPos_Searcher
	: public StrCharPos_Compare<CharT, GetCharElem, GetCharCode, CompareCharCode>
{
protected:
	typedef StrCharPos_Compare<CharT, GetCharElem, GetCharCode, CompareCharCode> super;
	GetStringEl m_str;

public:
	explicit StrCharPos_Searcher(
		  const GetCharElem& gch = GetCharElem()
		, const GetCharCode& code = GetCharCode()
		, const GetStringEl& gstr = GetStringEl()
		)
	: super(gch, code), m_str(gstr) {}

	template<class RanIt>
	std::pair<RanIt,RanIt>
	equal_range(RanIt _First, RanIt _Last, const CharT* word, int pos) const
	{
		CharT _WordCh = this->m_code(word[pos]);
		return equal_range(_First, _Last, _WordCh, pos);
	}
	template<class RanIt>
	RanIt
	lower_bound(RanIt _First, RanIt _Last, const CharT* word, int pos) const
	{
		CharT _WordCh = this->m_code(word[pos]);
		return lower_bound(_First, _Last, _WordCh, pos);
	}
	template<class RanIt>
	RanIt
	upper_bound(RanIt _First, RanIt _Last, const CharT* word, int pos) const
	{
		CharT _WordCh = this->m_code(word[pos]);
		return upper_bound(_First, _Last, _WordCh, pos);
	}

protected:
	template<class Any>
	CharT code(const Any& x, int pos) const { return this->m_code(this->m_gch(m_str(x), pos)); }

	using super::code;

	template<class RanIt>
	std::pair<RanIt,RanIt>
	equal_range(RanIt _First, RanIt _Last, const CharT _WordCh, int pos) const
	{
		assert(_Last - _First > 0);
		assert(this->compare(code(*_First, pos), code(*(_Last-1), pos)) <= 0);

		typedef typename std::iterator_traits<RanIt>::difference_type diff_t;
		diff_t _Count = _Last - _First;
		for (; 0 < _Count; )
		{	// divide and conquer, check midpoint
			diff_t _Count2 = _Count / 2;
			RanIt _Mid = _First;
			std::advance(_Mid, _Count2);

			CharT _MidCh = code(*_Mid, pos);

			if (CompareCharCode::operator()(_MidCh, _WordCh))
			{	// range begins above _Mid, loop
				_First = ++_Mid;
				_Count -= _Count2 + 1;
			}
			else if (CompareCharCode::operator()(_WordCh, _MidCh))
				_Count = _Count2;	// range in first half, loop
			else
			{	// range straddles _Mid, find each end and return
				RanIt _First2 = this->lower_bound(_First, _Mid, _WordCh, pos);
				std::advance(_First, _Count);
				RanIt _Last2 = this->upper_bound(++_Mid, _First, _WordCh, pos);
				return (std::pair<RanIt,RanIt>(_First2, _Last2));
			}
		}
		return (std::pair<RanIt,RanIt>(_First, _First));	// empty range
	}

	template<class RanIt>
	RanIt lower_bound(RanIt _First, RanIt _Last, const CharT _WordCh, int pos) const
	{
		typedef typename std::iterator_traits<RanIt>::difference_type diff_t;
		diff_t _Count = _Last - _First;

		for (; 0 < _Count; )
		{	// divide and conquer, find half that contains answer
			diff_t _Count2 = _Count / 2;
			RanIt _Mid = _First;
			std::advance(_Mid, _Count2);

			if (CompareCharCode::operator()(code(*_Mid, pos), _WordCh))
				_First = ++_Mid, _Count -= _Count2 + 1;
			else
				_Count = _Count2;
		}
		return (_First);
	}

	template<class RanIt>
	RanIt upper_bound(RanIt _First, RanIt _Last, const CharT _WordCh, int pos) const
	{
		typedef typename std::iterator_traits<RanIt>::difference_type diff_t;
		diff_t _Count = _Last - _First;

		for (; 0 < _Count; )
		{	// divide and conquer, find half that contains answer
			diff_t _Count2 = _Count / 2;
			RanIt _Mid = _First;
			std::advance(_Mid, _Count2);

			if (!CompareCharCode::operator()(_WordCh, code(*_Mid, pos)))
				_First = ++_Mid, _Count -= _Count2 + 1;
			else
				_Count = _Count2;
		}
		return (_First);
	}
};

} }

#endif // __terark_wordseg_search_h__
