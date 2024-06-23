/* vim: set tabstop=4 : */
// common member functions of ZStringTable and SortedTable
// 同一函数名，既有 const 又有 non-const 的那些函数

	TERARK_M_iterator begin() TERARK_M_const { return m_index.begin();   }
	TERARK_M_iterator end()   TERARK_M_const { return m_index.end() - 1; }

	TERARK_M_reverse_iterator rbegin() TERARK_M_const { return TERARK_M_reverse_iterator(end()); }
	TERARK_M_reverse_iterator rend()   TERARK_M_const { return TERARK_M_reverse_iterator(begin()); }

	/**
	 @{
	 @brief 将 iterator 转化成 handle

	 @note 如非必要，不要使用该函数
	 */
	handle_t to_handle(TERARK_M_iterator iter) TERARK_M_const
	{
		assert(end() != iter);
		return handle_t(iter - this->begin());
	}
	//@}

	/**
	 @{
	 @brief 将 handle 转化成 iterator
	 @note 如非必要，不要使用该函数
	 */
	TERARK_M_iterator to_iterator(handle_t handle) TERARK_M_const
	{
		assert(handle.t < size());
		return begin() + handle.t;
	}
	//@}

	TERARK_M_const value_type& front() TERARK_M_const
	{
		assert(m_index.size() > 1);
		return (m_index.begin())->val();
	}
	TERARK_M_const value_type& back() TERARK_M_const
	{
		assert(m_index.size() > 1);
		return (m_index.end()-2)->val();
	}

	TERARK_M_const value_type& operator[](difference_type idx) TERARK_M_const
	{
		assert(0 <= idx && idx < m_index.size()-1);
		return m_index[idx].val();
	}

//////////////////////////////////////////////////////////////////////////
	template<class CompatibleCompare>
	TERARK_M_iterator
		lower_bound(TERARK_M_iterator first, TERARK_M_iterator last,
					const key_type& xkey, CompatibleCompare comp) TERARK_M_const
	{
		return TERARK_PZ_FUN(lower_bound)(TERARK_PZ_SEARCH_FUN_ARG_LIST_II);
	}
	template<class CompatibleCompare>
	TERARK_M_iterator
		upper_bound(TERARK_M_iterator first, TERARK_M_iterator last,
					const key_type& xkey, CompatibleCompare comp) TERARK_M_const
	{
		return TERARK_PZ_FUN(upper_bound)(TERARK_PZ_SEARCH_FUN_ARG_LIST_II);
	}
	template<class CompatibleCompare>
	std::pair<TERARK_M_iterator, TERARK_M_iterator>
		equal_range(TERARK_M_iterator first, TERARK_M_iterator last,
					const key_type& xkey, CompatibleCompare comp) TERARK_M_const
	{
		return TERARK_PZ_FUN(equal_range)(TERARK_PZ_SEARCH_FUN_ARG_LIST_II);
	}

//////////////////////////////////////////////////////////////////////////

	template<class CompatibleCompare>
	TERARK_M_iterator lower_bound(const key_type& xkey, CompatibleCompare comp) TERARK_M_const
	{
		return TERARK_PZ_FUN(lower_bound)(TERARK_PZ_SEARCH_FUN_ARG_LIST_BE_1);
	}
	template<class CompatibleCompare>
	TERARK_M_iterator upper_bound(const key_type& xkey, CompatibleCompare comp) TERARK_M_const
	{
		return TERARK_PZ_FUN(upper_bound)(TERARK_PZ_SEARCH_FUN_ARG_LIST_BE_1);
	}
	template<class CompatibleCompare>
	std::pair<TERARK_M_iterator, TERARK_M_iterator>
		equal_range(const key_type& xkey, CompatibleCompare comp) TERARK_M_const
	{
		return TERARK_PZ_FUN(equal_range)(TERARK_PZ_SEARCH_FUN_ARG_LIST_BE_1);
	}

	//////////////////////////////////////////////////////////////////////////

	TERARK_M_iterator lower_bound(const key_type& xkey) TERARK_M_const
	{
		return TERARK_PZ_FUN(lower_bound)(TERARK_PZ_SEARCH_FUN_ARG_LIST_BE_0);
	}
	TERARK_M_iterator upper_bound(const key_type& xkey) TERARK_M_const
	{
		return TERARK_PZ_FUN(upper_bound)(TERARK_PZ_SEARCH_FUN_ARG_LIST_BE_0);
	}
	std::pair<TERARK_M_iterator, TERARK_M_iterator>
		equal_range(const key_type& xkey) TERARK_M_const
	{
		return TERARK_PZ_FUN(equal_range)(TERARK_PZ_SEARCH_FUN_ARG_LIST_BE_0);
	}

	//////////////////////////////////////////////////////////////////////////

	/**
	 @brief 计算两个 iter 的距离

	  对于这个类(SortedTable/ZStringTable)，只需简单相减
	  但是对于其它类，可能需要较复杂的计算

     @note lesser 和 larger 必须是每个 (key, {values}) 的第一个
	 */
	std::ptrdiff_t distance(TERARK_M_iterator lesser, TERARK_M_iterator larger) TERARK_M_const
	{
		assert(lesser < larger);
		return larger - lesser;
	}

	//////////////////////////////////////////////////////////////////////////

	template<class ValueCompare>
	TERARK_M_iterator
		value_lower_bound(TERARK_M_iterator ilow, TERARK_M_iterator iupp,
						  const value_type& val, ValueCompare vcomp) TERARK_M_const
	{
		assert(ilow < iupp);
		return std::lower_bound(ilow, iupp, val, vcomp);
	}
	template<class ValueCompare>
	TERARK_M_iterator
		value_upper_bound(TERARK_M_iterator ilow, TERARK_M_iterator iupp,
						  const value_type& val, ValueCompare vcomp) TERARK_M_const
	{
		assert(ilow < iupp);
		return std::upper_bound(ilow, iupp, val, vcomp);
	}
	template<class ValueCompare>
	std::pair<TERARK_M_iterator, TERARK_M_iterator>
		value_equal_range(TERARK_M_iterator ilow, TERARK_M_iterator iupp,
						  const value_type& val, ValueCompare vcomp) TERARK_M_const
	{
		assert(ilow < iupp);
		return std::equal_range(ilow, iupp, val, vcomp);
	}

#ifndef TERARK_IS_IN_ZSTRING_TABLE
	/**
	 @brief return first_equal

	  first_equal is the iterator which key equal to the key iter point to
	 */
	TERARK_M_iterator first_equal(TERARK_M_iterator iter) TERARK_M_const
	{
		assert(this->end() != iter);
		return lower_bound_near(m_index.begin(), iter.m_iter, *iter.m_iter,
			CompareNodeKey<key_type, key_compare>(m_comp));
	}

	/**
	 @brief key(iter) 是否与 key(iter+1) 相等

	 @note 如果 iter 的下一个是 this->end()，则返回 false
	 */
	bool equal_next(TERARK_M_iterator iter) TERARK_M_const
	{
		assert(iter.m_iter < m_index.end() - 1);
		if (iter.m_iter + 2 == m_index.end())
			return false;
		return !m_comp(iter.m_iter->key(), iter.m_iter[1].key());
	}

	size_t goto_next_larger(TERARK_M_iterator& iter) TERARK_M_const
	{
		typename index_t::TERARK_M_iterator iupper =
			upper_bound_near(iter.m_iter, m_index.end()-1, (*iter.m_iter).key(),
				CompareNodeKey<key_type, KeyCompareT>(m_comp));
		size_t count = iupper - iter.m_iter;
		iter.m_iter = iupper;
		return count;
	}

#endif //#ifndef TERARK_IS_IN_ZSTRING_TABLE

	TERARK_M_iterator
		find(TERARK_M_iterator first, TERARK_M_iterator last,
			 const key_type& xkey) TERARK_M_const
	{
		TERARK_M_iterator iter = this->lower_bound(first, last, xkey, m_comp);
		return iter != last && !m_comp(xkey, this->key(iter)) ? iter : end();
	}

	TERARK_M_iterator find(const key_type& xkey) TERARK_M_const
	{
		return find(begin(), end(), xkey);
	}

	/**
	 @{
	 @brief return next_larger iterator

	 next_larger iterator point to the first xkey-value which xkey is larger than the xkey iter point to
	 */
	TERARK_M_iterator next_larger(TERARK_M_iterator iter) TERARK_M_const
	{
		goto_next_larger(iter);
		return iter;
	}
	//@}

	template<class ResultContainer>
	void query_result(const TERARK_M_iterator& iter, ResultContainer& result) TERARK_M_const
	{
		assert(this->end() != iter);
		result.insert(result.end(), iter, next_larger(iter));
	}

#undef TERARK_M_reverse_iterator
#undef TERARK_M_iterator
#undef TERARK_M_const

