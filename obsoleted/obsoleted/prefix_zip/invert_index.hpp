/* vim: set tabstop=4 : */
#ifndef __terark_invert_index_h__
#define __terark_invert_index_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
# pragma warning(push)
# pragma warning(disable: 4018) // '<' : signed/unsigned mismatch
#endif

#include <boost/type_traits/remove_pointer.hpp>
//#include <boost/mpl/map.hpp>

#include "packed_table.hpp"
#include "sorted_table.hpp"
#include "biway_table.hpp"
#include "multi_sorted_table.hpp"
#include "serialize_common.hpp"

namespace terark {

//template<class KeyT, class ValueT, class KeyCompareT, class ValueCompareT>
template<class IndexT, class DataT, class ValueCompareT>
class InvertIndex
{
public:
	typedef IndexT index_t;
	typedef typename IndexT::  key_type    key_type;
	typedef typename IndexT::value_type offset_type;

	typedef typename IndexT::key_type key_t1;
	typedef typename DataT ::key_type key_t2;

	typedef typename DataT ::value_type value_type;

	typedef key_t1* iterator;
	typedef key_t2* iterator_2;

	iterator begin()
	{
		return 0;
	}
	iterator end()
	{
		return 0;
	}

	iterator_2 begin(iterator iter)
	{
		return iter->begin();
	}
	iterator_2 end(iterator iter)
	{
		return iter->end();
	}
};

} // namespace terark

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma warning(pop)
#endif

#endif // __terark_invert_index_h__

