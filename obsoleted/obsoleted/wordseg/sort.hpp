/* vim: set tabstop=4 : */
#ifndef __terark_wordseg_sort_h__
#define __terark_wordseg_sort_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

#include <vector>

#include "compare.hpp"
#include "../sort3.hpp"

//#include <boost/multi_index/identity.hpp>

//#define _STR_CHAR_POS_SORTER_PRINT_PARAM

#ifdef _STR_CHAR_POS_SORTER_PRINT_PARAM
#include <iostream>
#include <fstream>
#include <iomanip>
#include <map>
#endif

namespace terark { namespace wordseg {

#define TERARK_DEBUG_LT_PRED(pred, x, y) pred(x, y, pos)

/**
 * @param CharT char type
 * @param GetCharElem get char by index from string
 * @param GetCharCode get char internal code, such as: indentity, lowercase, uppercase, ...
 * @param GetStringEl return a string like interface object
 */
template< class CharT
		, class GetCharElem = GetCharFromBegin
		, class GetCharCode = XIdentity
		, class GetStringEl = XIdentity
		, class CompareCharCode = CharLess
		>
class StrCharPos_Sorter
{
	typedef StrCharPos_Compare<CharT, GetCharElem, GetCharCode, CompareCharCode> comp_t;
	size_t      m_max_prefix;
	comp_t      m_comp;
	GetStringEl m_str;

public:
	typedef CharT char_t;
	typedef basic_const_string<CharT> const_str_t;
	typedef std::basic_string<CharT> str_t;

	explicit StrCharPos_Sorter(int max_prefix
		, const GetCharElem& gch  = GetCharElem()
		, const GetCharCode& code = GetCharCode()
		, const GetStringEl& gstr = GetStringEl()
		)
	: m_max_prefix(max_prefix), m_comp(gch, code), m_str(gstr) {}

	template<class _RanIt>
	void sort(_RanIt _First, _RanIt _Last)
	{
		assert(_First <= _Last);
		if (_Last - _First <= 1)
			return;
		if (m_max_prefix >= 1024) {
			sort_c2(_First, _Last);
		} else {
			OpenDbgFile("sort_loop", m_str(*_First).begin());
			sort_loop(_First, _Last, 0);
		}
// 		StrCharPos_GenStrCmp<CharT, GetCharElem, GetCharCode, GetStringEl, CompareCharCode>
// 			cmp(pos, m_max_prefix, m_comp, m_str);
// 		terark::_Sort(_First, _Last, _Cnt, cmp);
	}

	template<class _RanIt> inline
	_RanIt _NextGreater(_RanIt _First, _RanIt _Last, size_t pos)
	{
		CharT ch = code(*_First, pos);
		const int _Cnt = _Last - _First;
		if (_Cnt > 512)
		{
			StrCharPos_Compare_Adapter<StrCharPos_Sorter> comp(*this, pos);
			_RanIt _GMid = _First + _Cnt / 8; // Guessed Mid
			_RanIt _Next = std::upper_bound(++_First, _GMid, ch, comp);
			if (_Next == _GMid)
				return std::upper_bound(_Next, _Last, ch, comp);
			else
				return _Next;
		}
		else
		{
			for (++_First; _First != _Last && ch == code(*_First, pos); ++_First)
				;
			return _First;
		}
	}

#ifdef _STR_CHAR_POS_SORTER_PRINT_PARAM
	template<class _RanIt>
	void PrintParam(_RanIt _First, _RanIt _Last, size_t pos)
	{
		using namespace std;
		m_nth_call++;
	//	for (int i = 0; i < pos; ++i)
	//		m_dbg_file << " ";
		m_dbg_file
			<< setw(5) << m_nth_call << ": "
			<< "(" << setw(5) << (_Last - _First)
			<< ", "
			<< "(" << setw(5) << (m_str(*_First).begin() - m_text_begin)
			<< "," << setw(5) << (m_str(*_First).end()   - m_text_begin)
			<< "), "
			<< "(" << setw(5) << (m_str(*(_Last-1)).begin() - m_text_begin)
			<< "," << setw(5) << (m_str(*(_Last-1)).end()   - m_text_begin)
			<< "), "
			<< setw(3) << pos
			<< ")\n";
	}
	void OpenDbgFile(const char* func, const CharT* text_begin)
	{
		string_appender<> oss;
		oss << func << "-" << m_nth_file[func]++ << ".txt";
		if (m_dbg_file.is_open())
			m_dbg_file.close();
		m_dbg_file.open(oss.str().c_str());
		m_dbg_file << "nth_call: (RangeSize, (_First[0]), (_Last[-1]), pos)\n";
		m_text_begin = text_begin;
		m_nth_call = 0;
	}
	int m_nth_call;
	const CharT* m_text_begin;
	std::ofstream m_dbg_file;
#else
	template<class _RanIt>
	void PrintParam(_RanIt _First, _RanIt _Last, size_t pos) {}
	void OpenDbgFile(const char* fname, const CharT* text_begin) {}
#endif

	//! 使用 bool 变量 bRecursive 标识不同的跳转地址
	//! 使用手工递归似乎比直接递归还要慢一点（2%左右）
	//! 但是手工递归能允许很深的递归，在堆栈内存的用量上也比直接递归要少
	template<class _RanIt>
	void sort_c(_RanIt _First, _RanIt _Last)
	{
		using namespace std;

		OpenDbgFile("sort_c", m_str(*_First).begin());

		vector<pair<_RanIt,_RanIt> > stack;stack.reserve(m_max_prefix);
		stack.push_back(make_pair(_First, _Last));
		bool bRecursive = true;
		while (!stack.empty())
		{
		LoopBegin:
			_First = stack.back().first;
			_Last = stack.back().second;
			if (bRecursive)
			{
				size_t pos = stack.size()-1;
				PrintParam(_First, _Last, pos);
				_Sort(_First, _Last, _Last - _First, pos);
				for (; _First != _Last && m_str(*_First).size() == pos; ++_First)
					assert(1);
			}
			for (; _First != _Last;)
			{
				size_t pos = stack.size()-1;
				_RanIt _First0 = _First;
			//	_First = _NextGreater(_First, _Last, pos);
				{
					CharT ch = code(*_First, pos);
					for (++_First; _First != _Last && ch == code(*_First, pos); ++_First)
						;
				}
				stack.back().first = _First;
				int _Cnt = _First - _First0;
				assert(_Cnt > 0);
				if (2 == _Cnt)
					sort_xy(*_First0, *(_First0+1), pos);
				else if (1 == _Cnt)
					; // do nothing...
				else if (stack.size() < stack.capacity())
				{
					stack.push_back(make_pair(_First0, _First));
					bRecursive = true;
					goto LoopBegin;
				}
			}
			bRecursive = false;
			stack.pop_back();
		}
	}

	//! 使用 Label 标识不同的跳转地址
	//! 使用手工递归似乎比直接递归还要慢一点（2%左右）
	template<class _RanIt>
	void sort_c2(_RanIt _First, _RanIt _Last)
	{
		using namespace std;

		OpenDbgFile("sort_c2", m_str(*_First).begin());

		vector<pair<_RanIt,_RanIt> > stack;stack.reserve(m_max_prefix);
		stack.push_back(make_pair(_First, _Last));
	Recursive:
		{
			size_t pos = stack.size()-1;
			PrintParam(_First, _Last, pos);
			_Sort(_First, _Last, _Last - _First, pos);
			for (; _First != _Last && m_str(*_First).size() == pos; ++_First)
				assert(1);
		}
	LoopOnly:
		for (; _First != _Last;)
		{
			size_t pos = stack.size()-1;
			_RanIt _First0 = _First;
		//	_First = _NextGreater(_First, _Last, pos);
			{
				CharT ch = code(*_First, pos);
				for (++_First; _First != _Last && ch == code(*_First, pos); ++_First)
					;
			}
			stack.back().first = _First;
			int _Cnt = _First - _First0;
			assert(_Cnt > 0);
			if (2 == _Cnt)
				sort_xy(*_First0, *(_First0+1), pos);
			else if (1 == _Cnt)
				; // do nothing...
			else if (stack.size() < stack.capacity())
			{
				stack.push_back(make_pair(_First0, _First));
				_Last = _First;
				_First = _First0;
				goto Recursive;
			}
		}
		stack.pop_back();
		if (!stack.empty())
		{
			_First = stack.back().first;
			_Last = stack.back().second;
			goto LoopOnly;
		}
	}

	template<class Any> inline
	CharT code(const Any& x, size_t pos) const { return m_comp.code(m_str(x), pos); }

	template<class Any> inline
	bool bi_comp(const Any& x, const Any& y, size_t pos) const
	{
		assert(int(m_str(x).size()) > pos);
		assert(int(m_str(y).size()) > pos);
		return m_comp.bi_comp(m_str(x), m_str(y), pos);
	}
	template<class Any> inline
	bool bi_comp(const CharT x, const Any& y, size_t pos) const
	{
		assert(int(m_str(y).size()) > pos);
		return m_comp(x, code(y, pos));
	}
	template<class Any> inline
	bool bi_comp(const Any& x, const CharT y, size_t pos) const
	{
		assert(int(m_str(x).size()) > pos);
		return m_comp(code(x, pos), y);
	}

	template<class Any>
	bool checked_bi_comp(const Any& x, const Any& y, size_t pos) const
	{
		assert(m_str(x).size() >= pos);
		assert(m_str(y).size() >= pos);

		if (m_str(x).size() == pos) {
			if (m_str(y).size() == pos)
				return false;
			else // x < y
				return true;
		}
		else if (m_str(y).size() == pos) {
			// x > y
			return false;
		} else
			return m_comp.bi_comp(m_str(x), m_str(y), pos);
	}
	template<class Any>
	bool checked_bi_comp(const CharT x, const Any& y, size_t pos) const
	{ // x is code(string_x), so string_x.size() > pos
		assert(m_str(y).size() >= pos);
		if (m_str(y).size() == pos)
			return false; // x > y
		else
			return m_comp(x, code(y, pos));
	}
	template<class Any>
	bool checked_bi_comp(const Any& x, const CharT y, size_t pos) const
	{ // y is code(string_y), so string_y.size() > pos
		assert(int(m_str(x).size()) >= pos);
		if (m_str(x).size() == pos)
			return true; // x < y
		else
			return m_comp(code(x, pos), y);
	}

	template<class Any>
	int compare(const Any& x, const Any& y, size_t pos) const
	{
		assert(int(m_str(x).size()) >= pos);
		assert(int(m_str(y).size()) >= pos);

		if (m_str(x).size() == pos) {
			if (m_str(y).size() == pos)
				return 0;
			else // x < y
				return 1;
		}
		else if (m_str(y).size() == pos) {
			// x > y
			return -1;
		} else
			return m_comp.compare(m_str(x), m_str(y));
	}

protected:

#define BRANCH_BY_COMPARE_SS_2(x, y, less_branch, equal_greater_branch) \
		if (m_str(x).size() == pos) {   \
			if (m_str(y).size() == pos) \
			{ equal_greater_branch; }	\
			else						\
			{ less_branch;			}   \
		}								\
		else if (m_str(y).size() == pos) { \
			equal_greater_branch;		\
		}								\
		else if (m_comp.bi_comp(m_str(x), m_str(y), pos)) {	\
			less_branch;				\
		} else {						\
			equal_greater_branch;		\
		}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#define BRANCH_BY_COMPARE_SS_3(x, y, less_branch, greater_branch, equal_branch) \
		if (m_str(x).size() == pos) {   \
			if (m_str(y).size() == pos) \
				equal_branch;			\
			else						\
				less_branch;			\
		}								\
		else if (m_str(y).size() == pos) { \
			greater_branch;				\
		} else {						\
			int comp = m_comp.compare(m_str(x), m_str(y), pos); \
			if (comp < 0)				\
				less_branch;			\
			else if (comp > 0)			\
				greater_branch;			\
			else						\
				equal_branch;			\
		}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

	template<class _BidIt> inline
	void _Insertion_sort(_BidIt _First, _BidIt _Last, size_t pos)
	{	// insertion sort [_First, _Last), using _Pred
		if (_First != _Last)
			for (_BidIt _Next = _First; ++_Next != _Last; )
			{	// order next element
				_BidIt _Next1 = _Next;
				typename std::iterator_traits<_BidIt>::value_type _Val = *_Next;
				CharT ch;

				if (m_str(_Val).size() == pos ||
					(ch = code(_Val, pos), TERARK_DEBUG_LT_PRED(checked_bi_comp, ch, *_First)))
				{	// found new earliest element, move to front
					// _Val < *_First
					std::copy_backward(_First, _Next, ++_Next1);
					*_First = _Val;
				}
				else
				{	// look for insertion point after first
					for (_BidIt _First1 = _Next1;
						 TERARK_DEBUG_LT_PRED(checked_bi_comp, ch, *--_First1);
						 _Next1 = _First1)
						*_Next1 = *_First1;	// move hole down
					*_Next1 = _Val;	// insert element in hole
				}
			}
	}

	template<class _RanIt> inline
	void _Med3(_RanIt _First, _RanIt _Mid, _RanIt _Last, size_t pos)
	{	// sort median of three elements to middle
		BRANCH_BY_COMPARE_SS_2(*_Mid, *_First, std::iter_swap(_Mid, _First), (void)0);
		BRANCH_BY_COMPARE_SS_2(*_Last, *_Mid , std::iter_swap(_Last, _Mid ), (void)0);
		BRANCH_BY_COMPARE_SS_2(*_Mid, *_First, std::iter_swap(_Mid, _First), (void)0);
	}

	template<class _RanIt> inline
	void _Median(_RanIt _First, _RanIt _Mid, _RanIt _Last, size_t pos)
	{	// sort median element to middle
		if (40 < _Last - _First)
		{	// median of nine
			size_t _Step = (_Last - _First + 1) / 8;
			_Med3(_First, _First + _Step, _First + 2 * _Step, pos);
			_Med3(_Mid - _Step, _Mid, _Mid + _Step, pos);
			_Med3(_Last - 2 * _Step, _Last - _Step, _Last, pos);
			_Med3(_First + _Step, _Mid, _Last - _Step, pos);
		}
		else
			_Med3(_First, _Mid, _Last, pos);
	}

	template<class _RanIt> inline
	std::pair<_RanIt, _RanIt> _Unguarded_partition(_RanIt _First, _RanIt _Last, size_t pos)
	{	// partition [_First, _Last), using _Pred
		_RanIt _Mid = _First + (_Last - _First) / 2;
		_Median(_First, _Mid, _Last - 1, pos);
		_RanIt _Pfirst = _Mid;
		_RanIt _Plast = _Pfirst + 1;

		while (_First < _Pfirst)
		{
			BRANCH_BY_COMPARE_SS_3(*(_Pfirst - 1), *_Pfirst, break, break, --_Pfirst);
		}
		while (_Plast < _Last)
		{
			BRANCH_BY_COMPARE_SS_3(*_Plast, *_Pfirst, break, break, ++_Plast);
		}

		_RanIt _Gfirst = _Plast;
		_RanIt _Glast = _Pfirst;

		for (; ; )
		{	// partition
			for (; _Gfirst < _Last; ++_Gfirst)
			{
				BRANCH_BY_COMPARE_SS_3(*_Pfirst, *_Gfirst,
					(void)NULL, break, std::iter_swap(_Plast++, _Gfirst));
			}
			for (; _First < _Glast; --_Glast)
			{
				BRANCH_BY_COMPARE_SS_3(*(_Glast - 1), *_Pfirst,
					(void)NULL, break, std::iter_swap(--_Pfirst, _Glast - 1));
			}
			if (_Glast == _First && _Gfirst == _Last)
				return (std::pair<_RanIt, _RanIt>(_Pfirst, _Plast));

			if (_Glast == _First)
			{	// no room at bottom, rotate pivot upward
				if (_Plast != _Gfirst)
					std::iter_swap(_Pfirst, _Plast);
				++_Plast;
				std::iter_swap(_Pfirst++, _Gfirst++);
			}
			else if (_Gfirst == _Last)
			{	// no room at top, rotate pivot downward
				if (--_Glast != --_Pfirst)
					std::iter_swap(_Glast, _Pfirst);
				std::iter_swap(_Pfirst, --_Plast);
			}
			else
				std::iter_swap(_Gfirst++, --_Glast);
		}
	}

	template<class _RanIt, class _Diff> inline
	void _Sort(_RanIt _First, _RanIt _Last, _Diff _Ideal, size_t pos)
	{	// order [_First, _Last), using _Pred
		_Diff _Count;
		for (; _ISORT_MAX < (_Count = _Last - _First) && 0 < _Ideal; )
		{	// divide and conquer by quicksort
			std::pair<_RanIt, _RanIt> _Mid = _Unguarded_partition(_First, _Last, pos);
			_Ideal /= 2, _Ideal += _Ideal / 2;	// allow 1.5 log2(N) divisions

			if (_Mid.first - _First < _Last - _Mid.second)
			{	// loop on second half
				_Sort(_First, _Mid.first, _Ideal, pos);
				_First = _Mid.second;
			}
			else
			{	// loop on first half
				_Sort(_Mid.second, _Last, _Ideal, pos);
				_Last = _Mid.first;
			}
		}

		if (_ISORT_MAX < _Count)
		{	// heap sort if too many divisions
			StrCharPos_Compare_Adapter_Checked<StrCharPos_Sorter> comp(*this, pos);
			std::make_heap(_First, _Last, comp);
			std::sort_heap(_First, _Last, comp);
		}
		else if (1 < _Count)
			_Insertion_sort(_First, _Last, pos);	// small
	}

	template<class _Val> inline
	int sort_xy(_Val& x, _Val& y, size_t pos)
	{
		using namespace std; // for min
		int size = min(m_str(x).size(), m_str(y).size());
		int idx = pos + 1;
		while (idx < size)
		{
			CharT c1, c2;
			if ((c1 = code(x, idx)) != (c2 = code(y, idx)))
			{
				if (m_comp(c2, c1))
					std::swap(x, y);
				return idx;
			}
			++idx;
		}
		if (m_str(x).size() > m_str(y).size())
			std::swap(x, y);
		return idx;
	}

	template<class _RanIt>
	void sort_loop(_RanIt _First, _RanIt _Last, const size_t pos)
	{
		if (pos == m_max_prefix)
			return;

		PrintParam(_First, _Last, pos);

#if defined(_DEBUG) || !defined(NDEBUG)
		int _DbgCnt = _Last - _First;
		if (pos > 0)
			assert(_DbgCnt >= 3);

		for (_RanIt iter = _First; iter != _Last; ++iter)
		{
			typename std::iterator_traits<_RanIt>::value_type& v = *iter;
		//	int nth = iter - _First;
			size_t len = m_str(v).size();
			assert(len >= pos);
		}
		_RanIt _DbgFirst = _First;
#endif
		{
// 			int _Cnt = _Last - _First;
// 			if (_Cnt < 256)
// 			{
// 				StrCharPos_GenStrCmp<CharT, GetCharElem, GetCharCode, GetStringEl, CompareCharCode>
// 					cmp(pos, m_max_prefix, m_comp, m_str);
// 				terark::_Sort(_First, _Last, _Cnt, cmp);
// 				return;
// 			}
// 			else
				this->_Sort(_First, _Last, _Last - _First, pos);
		}
		// skip the min string, these string.size == pos
		for (; _First != _Last && m_str(*_First).size() == pos; ++_First)
			assert(1);

#if defined(_DEBUG) || !defined(NDEBUG)
		int _DbgSkipped = _First - _DbgFirst; (void)_DbgSkipped;
#endif
		for (; _First != _Last;)
		{
			_RanIt _First0 = _First;
// 			_First = _NextGreater(_First, _Last, pos);
			{
				CharT ch = code(*_First, pos);
				for (++_First; _First != _Last && ch == code(*_First, pos); ++_First)
					;
			}
			int _Cnt = _First - _First0;
			assert(_Cnt > 0);
			if (2 == _Cnt)
				sort_xy(*_First0, *(_First0+1), pos);
			else if (1 == _Cnt)
				; // do nothing...
			else
				sort_loop(_First0, _First, pos + 1);
		}
	}
};

} }

#endif // __terark_wordseg_sort_h__
