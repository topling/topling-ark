/* vim: set tabstop=4 : */
#ifndef __terark_ext_index_h__
#define __terark_ext_index_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

#include <terark/io/DataIO.hpp>

namespace terark { namespace prefix_zip {

template<class TableT, class IndexElement>
struct ExtIndexType;


/**
 @brief find lower_bound in [first, iter)
 @note return value is in [first, iter)
 */
template<class IterT, class ValueT, class CompareT>
IterT lower_bound_near(IterT first, IterT iter, const ValueT& val, CompareT comp)
{
	typename IterT::difference_type len = 1;
	while (iter - first > len)
	{
		IterT middle = iter - len;
		if (comp(*middle, val))
			// *middle < val, val > *middle
			return std::lower_bound(++middle, ++iter, val, comp);
		else {
			// !(*middle < val), middle >= val, val <= middle
			len *= 2;
			iter = middle;
		}
	}
	return std::lower_bound(first, iter, val, comp);
}

template<class IterT, class ValueT, class CompareT>
IterT lower_bound_near(IterT first, IterT iter, const ValueT& val, typename IterT::difference_type predictedDiff, CompareT comp)
{
	using namespace std;

	typename IterT::difference_type len = min(iter - first, predictedDiff); // 不同之处仅在这一行
	while (iter - first > len)
	{
		IterT middle = iter - len;
		if (comp(*middle, val))
			// *middle < val, val > *middle
			return std::lower_bound(++middle, ++iter, val, comp);
		else {
			// !(*middle < val), middle >= val, val <= middle
			len *= 2;
			iter = middle;
		}
	}
	return std::lower_bound(first, iter, val, comp);
}

/**
 @brief find upper_bound near 'iter', total upper_bound is 'last'

 @note return value is in [iter+1, last)
 */
template<class IterT, class ValueT, class CompareT>
IterT upper_bound_near(IterT iter, IterT last, const ValueT& val, CompareT comp)
{
	typename IterT::difference_type len = 1;
	while (last - iter > len)
	{
		IterT middle = iter + len;
		if (comp(val, *middle))
		{
			// val < *middle, *middle > val
			return std::upper_bound(iter, middle, val, comp);
		}
		else // !(val < middle) val >= middle, middle <= val
		{
			len *= 2;
			iter = middle;
		}
	}
	return std::upper_bound(iter, last, val, comp);
}

} } // namespace terark::prefix_zip


#endif // __terark_ext_index_h__
