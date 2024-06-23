/* vim: set tabstop=4 : */
// common member functions of ZStringTable and SortedTable
// 同一函数名，只有 const 或者 non-const 的那些函数

	BOOST_STRONG_TYPEDEF(uint32_t, handle_t);

//////////////////////////////////////////////////////////////////////////
/// @name non-const operations @{
#define TERARK_M_const
#define TERARK_M_iterator iterator
#define TERARK_M_reverse_iterator reverse_iterator
#include "sorted_table_mf.hpp"
/// @}

/// @name const operations @{
#define TERARK_M_const const
#define TERARK_M_iterator const_iterator
#define TERARK_M_reverse_iterator const_reverse_iterator
#include "sorted_table_mf.hpp"
/// @}
#undef TERARK_PZ_SEARCH_FUN_ARG_LIST_II
#undef TERARK_PZ_SEARCH_FUN_ARG_LIST_BE_1
#undef TERARK_PZ_SEARCH_FUN_ARG_LIST_BE_0
#undef TERARK_PZ_FUN
//////////////////////////////////////////////////////////////////////////

	void reserve(size_type indexSize) { m_index.reserve(indexSize+1); }

	key_type key(handle_t handle) const
	{
		assert(handle.t < this->size());
		return key(begin() + handle.t);
	}

	key_compare comp() const { return m_comp; }

	bool empty() const
	{
		assert(m_index.size() >= 1);
		return m_index.size() == 1;
	}
	size_type size() const	{ return m_index.size() - 1; }

	//! same as size
	size_type val_count() const { return m_index.size() - 1; }

	/**
	 @{
	 @brief 为 iter 指向的 xkey 关联的 values 排序

	 @return 返回排序的 value_count
	 @note iter 必须指向 xkey-values 中的第一个 value
	 */
	template<class ValueCompare>
	size_t sort_values(iterator iter, ValueCompare vcomp)
	{
	//	BOOST_STATIC_ASSERT(!is_unique_key_t::value); // unique xkey has no sort
		assert(iter >= begin() && iter < end());

		iterator next = iter;
		size_t count = goto_next_larger(next);
		std::sort(iter.m_iter, next.m_iter, IndexNode_CompValue<node_t, ValueCompare>(vcomp));

		return count;
	}
	template<class ValueCompare>
	size_t sort_values(iterator ilow, iterator iupp, ValueCompare vcomp)
	{
	//	BOOST_STATIC_ASSERT(!is_unique_key_t::value); // unique xkey has no sort
		assert(ilow < iupp);
		assert(ilow >= begin() && iter <  end());

		std::sort(ilow.m_iter, next.m_iter, IndexNode_CompValue<node_t, ValueCompare>(vcomp));

		return count;
	}
	template<class ValueCompare>
	size_t sort_values(handle_t handle, ValueCompare vcomp)
	{
	//	BOOST_STATIC_ASSERT(!is_unique_key_t::value); // unique xkey has no sort
		assert(handle.t < size());
		iterator iter = begin() + handle.t;
		return sort_values(iter, vcomp);
	}
	//@}

	//! sort all values associated its xkey
	//!
	//! @return same as key_count
	template<class ValueCompare>
	size_t sort_values(ValueCompare vcomp)
	{
	//	BOOST_STATIC_ASSERT(!is_unique_key_t::value); // unique xkey has no sort

		typename index_t::iterator iter = m_index.begin();
		size_t count = 0;
		while (iter != m_index.end()-1)
		{
		//	typename index_t::iterator upper = std::upper_bound(iter, m_index.end()-1, *iter, typename node_t::CompOffset());
			typename index_t::iterator upper = upper_bound_near(iter, m_index.end()-1, *iter, typename node_t::CompOffset());
			std::sort(iter, upper, IndexNode_CompValue<node_t, ValueCompare>(vcomp));
			count += upper - iter;
			iter = upper;
		}
		return count;
	}

	/**
	 @{
	 @brief append the values which associated xkey is equal to 'xkey' by comp

	 @note these method will not erase original values in result
	 */
	template<class ResultContainer, class CompatibleCompare>
	void query_result(const key_type& xkey, ResultContainer& result, CompatibleCompare comp) const
	{
		copy_range(equal_range(xkey, comp), result);
	}

// 	template<class CompatibleCompare>
// 	std::vector<value_type> query_result(const key_type& xkey, const CompatibleCompare comp) const
// 	{
// 		std::vector<value_type> result;
// 		copy_range(equal_range(xkey, comp), result);
// 		return result;
// 	}

	template<class ResultContainer>
	void query_result(const key_type& xkey, ResultContainer& result) const
	{
		copy_range(equal_range(xkey), result);
	}

	std::vector<value_type> query_result(const key_type& xkey) const
	{
		std::vector<value_type> result;
		copy_range(equal_range(xkey), result);
		return result;
	}

	template<class ResultContainer>
	static void copy_range(const_range_t range, ResultContainer& result)
	{
		while (range.first.m_iter != range.second.m_iter)
		{
			result.push_back((*range.first.m_iter).val());
			++range.first.m_iter;
		}
	}
	//@}

	/**
	 @brief 计算两个 iter 的距离

	  对于这个类(SortedTable/ZStringTable)，只需简单相减
	  但是对于其它类，可能需要较复杂的计算

     @note lesser 和 larger 必须指向相同 key 的 value,
	       或者 iter2 指向 iter1 下一个 key 的第一个 value
	 */
	difference_type value_distance(const_iterator iter1, const_iterator iter2) const
	{
		assert(iter1 <= iter2);
#ifdef TERARK_IS_IN_ZSTRING_TABLE
//		assert(!iter1.m_iter->offset == iter2.m_iter.offset);
#else
//		assert(!m_comp(key(iter1), key(iter2)) && !m_comp(key(iter1), key(iter2)));
#endif
		return iter2 - iter1;
	}

