/* vim: set tabstop=4 : */
#ifndef __terark_sort3_h__
#define __terark_sort3_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

#include <algorithm>

namespace terark {

//! _Pred must has:
//! int _Pred::compare(x,y)
//!   if (x< y)  return < 0
//!   if (x==y)  return ==0
//!   if (x> y)  return > 0
//! bool _Pred.operator()(x, y)
//!   if (x< y)  return true; else return false;

#if defined(_DEBUG_LT_PRED)
#define _MS_DEBUG_LT_PRED _DEBUG_LT_PRED
#else
#define _MS_DEBUG_LT_PRED(pred, x, y) pred(x, y)
#endif

const int _ISORT_MAX = 32;	// maximum size for insertion sort

template<class _BidIt, class _Pr> inline
void _Insertion_sort(_BidIt _First, _BidIt _Last, _Pr _Pred)
{	// insertion sort [_First, _Last), using _Pred
	if (_First != _Last)
		for (_BidIt _Next = _First; ++_Next != _Last; )
		{	// order next element
			_BidIt _Next1 = _Next;
			typename std::iterator_traits<_BidIt>::value_type _Val = *_Next;

			if (_MS_DEBUG_LT_PRED(_Pred, _Val, *_First))
			{	// found new earliest element, move to front
				std::copy_backward(_First, _Next, ++_Next1);
				*_First = _Val;
			}
			else
			{	// look for insertion point after first
				for (_BidIt _First1 = _Next1;
					_MS_DEBUG_LT_PRED(_Pred, _Val, *--_First1);
					_Next1 = _First1)
					*_Next1 = *_First1;	// move hole down
				*_Next1 = _Val;	// insert element in hole
			}
		}
}

template<class _RanIt, class _Pr> inline
void _Med3(_RanIt _First, _RanIt _Mid, _RanIt _Last, _Pr _Pred)
{	// sort median of three elements to middle
	if (_MS_DEBUG_LT_PRED(_Pred, *_Mid, *_First))
		std::iter_swap(_Mid, _First);
	if (_MS_DEBUG_LT_PRED(_Pred, *_Last, *_Mid))
		std::iter_swap(_Last, _Mid);
	if (_MS_DEBUG_LT_PRED(_Pred, *_Mid, *_First))
		std::iter_swap(_Mid, _First);
}

template<class _RanIt, class _Pr> inline
void _Median(_RanIt _First, _RanIt _Mid, _RanIt _Last, _Pr _Pred)
{	// sort median element to middle
	if (40 < _Last - _First)
	{	// median of nine
		size_t _Step = (_Last - _First + 1) / 8;
		terark::_Med3(_First, _First + _Step, _First + 2 * _Step, _Pred);
		terark::_Med3(_Mid - _Step, _Mid, _Mid + _Step, _Pred);
		terark::_Med3(_Last - 2 * _Step, _Last - _Step, _Last, _Pred);
		terark::_Med3(_First + _Step, _Mid, _Last - _Step, _Pred);
	}
	else
		terark::_Med3(_First, _Mid, _Last, _Pred);
}

template<class _RanIt, class _Pr> inline
std::pair<_RanIt, _RanIt>
_Unguarded_partition(_RanIt _First, _RanIt _Last, _Pr _Pred)
{	// partition [_First, _Last), using _Pred
	_RanIt _Mid = _First + (_Last - _First) / 2;
	terark::_Median(_First, _Mid, _Last - 1, _Pred);
	_RanIt _Pfirst = _Mid;
	_RanIt _Plast = _Pfirst + 1;

	while (_First < _Pfirst && _Pred.compare(*(_Pfirst - 1), *_Pfirst) == 0)
		--_Pfirst;
	while (_Plast < _Last && _Pred.compare(*_Plast, *_Pfirst) == 0)
		++_Plast;

	_RanIt _Gfirst = _Plast;
	_RanIt _Glast = _Pfirst;

	for (; ; )
	{	// partition
		for (; _Gfirst < _Last; ++_Gfirst)
		{
			int cmp = _Pred.compare(*_Pfirst, *_Gfirst);
			if (cmp < 0)
				;
			else if (cmp > 0)
				break;
			else
				std::iter_swap(_Plast++, _Gfirst);
		}
		for (; _First < _Glast; --_Glast)
		{
			int cmp = _Pred.compare(*(_Glast - 1), *_Pfirst);
			if (cmp < 0)
				;
			else if (cmp > 0)
				break;
			else
				std::iter_swap(--_Pfirst, _Glast - 1);
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

template<class _RanIt, class _Diff, class _Pr> inline
void _Sort(_RanIt _First, _RanIt _Last, _Diff _Ideal, _Pr _Pred)
{	// order [_First, _Last), using _Pred
	_Diff _Count;
	for (; _ISORT_MAX < (_Count = _Last - _First) && 0 < _Ideal; )
	{	// divide and conquer by quicksort
		std::pair<_RanIt, _RanIt> _Mid =
			terark::_Unguarded_partition(_First, _Last, _Pred);
		_Ideal /= 2, _Ideal += _Ideal / 2;	// allow 1.5 log2(N) divisions

		if (_Mid.first - _First < _Last - _Mid.second)
		{	// loop on second half
			terark::_Sort(_First, _Mid.first, _Ideal, _Pred);
			_First = _Mid.second;
		}
		else
		{	// loop on first half
			terark::_Sort(_Mid.second, _Last, _Ideal, _Pred);
			_Last = _Mid.first;
		}
	}

	if (_ISORT_MAX < _Count)
	{	// heap sort if too many divisions
		std::make_heap(_First, _Last, _Pred);
		std::sort_heap(_First, _Last, _Pred);
	}
	else if (1 < _Count)
		terark::_Insertion_sort(_First, _Last, _Pred);	// small
}

template<class _RanIt, class _Pr> inline
void sort3(_RanIt _First, _RanIt _Last, _Pr _Pred)
{	// order [_First, _Last), using _Pred
#if defined(_DEBUG_RANGE) && defined(_DEBUG_POINTER) && defined(_CHECKED_BASE)
	_DEBUG_RANGE(_First, _Last);
	_DEBUG_POINTER(_Pred);
	terark::_Sort(_CHECKED_BASE(_First), _CHECKED_BASE(_Last), _Last - _First, _Pred);
#else
	terark::_Sort(_First, _Last, _Last - _First, _Pred);
#endif
}

template<class _FwdIt, class _Ty, class _Pr> inline
std::pair<_FwdIt, _FwdIt>
equal_range3(_FwdIt _First, _FwdIt _Last, const _Ty& _Val, _Pr _Pred)
{	// find range equivalent to _Val, using _Pred
#if defined(_DEBUG_POINTER) && defined(_DEBUG_ORDER_SINGLE_PRED)
	_DEBUG_POINTER(_Pred);
	_DEBUG_ORDER_SINGLE_PRED(_First, _Last, _Pred, true);
#endif
	typedef typename std::iterator_traits<_FwdIt>::difference_type diff_t;
	diff_t _Count = std::distance(_First, _Last);

	for (; 0 < _Count; )
	{	// divide and conquer, check midpoint
		diff_t _Count2 = _Count / 2;
		_FwdIt _Mid = _First;
		std::advance(_Mid, _Count2);

#if defined(_DEBUG_ORDER_SINGLE_PRED)
		_DEBUG_ORDER_SINGLE_PRED(_Mid, _Last, _Pred, false);
#endif
		int cmp = _Pred.compare(*_Mid, _Val);
		if (cmp < 0)
		{	// range begins above _Mid, loop
			_First = ++_Mid;
			_Count -= _Count2 + 1;
		}
		else if (cmp > 0)
			_Count = _Count2;	// range in first half, loop
		else
		{	// range straddles _Mid, find each end and return
			_FwdIt _First2 = std::lower_bound(_First, _Mid, _Val, _Pred);
			std::advance(_First, _Count);
			_FwdIt _Last2 = std::upper_bound(++_Mid, _First, _Val, _Pred);
			return (std::pair<_FwdIt, _FwdIt>(_First2, _Last2));
		}
	}
	return (std::pair<_FwdIt, _FwdIt>(_First, _First));	// empty range
}

} // namespace terark

#endif // __terark_sort3_h__

