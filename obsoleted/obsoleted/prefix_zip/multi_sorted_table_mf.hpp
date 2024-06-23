/* vim: set tabstop=4 : */

	/**
	 @{
	 @brief 将 handle 转化成 iterator

	 @note 如非必要，不要使用该函数，该函数的时间开销相当于一次字符串查找
	 */
	TERARK_M_iterator to_iterator(handle_t handle) TERARK_M_const
	{
		return TERARK_M_iterator(begin().m_iter + handle.t);
	}
	//@}

	/**
	 @{
	 @brief 将 iterator 转化成 handle

	 @note 如非必要，不要使用该函数，该函数的时间开销相当于一次字符串查找
	 */
	handle_t to_handle(const TERARK_M_iterator& iter) TERARK_M_const
	{
		return handle_t(iter.m_iter - begin().m_iter);
	}
	//@}

	TERARK_M_const index_t& getIndex() TERARK_M_const { return m_index; }

	TERARK_M_iterator begin() TERARK_M_const { return TERARK_M_iterator(this, m_index.begin()); }
	TERARK_M_iterator end()   TERARK_M_const { return TERARK_M_iterator(this, m_index.end()); }

	TERARK_M_reverse_iterator rbegin() TERARK_M_const { return TERARK_M_reverse_iterator(end()); }
	TERARK_M_reverse_iterator rend()   TERARK_M_const { return TERARK_M_reverse_iterator(begin()); }

	void key(TERARK_M_iterator iter, key_type& k) TERARK_M_const
	{
		m_index.key(iter.m_iter, k);
	}
	key_type key(TERARK_M_iterator iter) TERARK_M_const
	{
		return m_index.key(iter.m_iter);
	}
	raw_key_t raw_key(const TERARK_M_iterator& iter) TERARK_M_const
	{
		return m_index.raw_key(iter.m_iter);
	}

	void key(TERARK_M_reverse_iterator iter, key_type& k) TERARK_M_const
	{
		m_index.key(iter.m_iter, k);
	}
	key_type key(TERARK_M_reverse_iterator iter) TERARK_M_const
	{
		return m_index.key(iter.m_iter);
	}
	raw_key_t raw_key(const TERARK_M_reverse_iterator& iter) TERARK_M_const
	{
		return m_index.raw_key(iter.m_iter);
	}

//////////////////////////////////////////////////////////////////////////
	template<class CompatibleCompare>
	TERARK_M_iterator
	lower_bound(TERARK_M_iterator first, TERARK_M_iterator last,
				const key_type& key, CompatibleCompare comp) TERARK_M_const
	{
		return TERARK_M_iterator(this, m_index.lower_bound(first.m_iter, last.m_iter, key, comp));
	}

	template<class SelfT, class IterT, class CompatibleCompare>
	TERARK_M_iterator
	upper_bound(TERARK_M_iterator first, TERARK_M_iterator last,
				const key_type& key, CompatibleCompare comp) TERARK_M_const
	{
		return TERARK_M_iterator(this, m_index.upper_bound(first.m_iter, last.m_iter, key, comp));
	}

	template<class CompatibleCompare>
	std::pair<TERARK_M_iterator, TERARK_M_iterator>
	equal_range(TERARK_M_iterator first, TERARK_M_iterator last,
				const key_type& key, CompatibleCompare comp) TERARK_M_const
	{
		std::pair<typename index_t::TERARK_M_iterator,
				  typename index_t::TERARK_M_iterator>
		ii = m_index.equal_range(first.m_iter, last.m_iter, key, comp);
		return std::pair<TERARK_M_iterator, TERARK_M_iterator>(
						 TERARK_M_iterator(this, ii.first),
						 TERARK_M_iterator(this, ii.second));
	}

	template<class CompatibleCompare>
	TERARK_M_iterator
	lower_bound(const key_type& key, CompatibleCompare comp) TERARK_M_const
	{
		return TERARK_M_iterator(this, m_index.lower_bound(key, comp));
	}
	template<class CompatibleCompare>
	TERARK_M_iterator
	upper_bound(const key_type& key, CompatibleCompare comp) TERARK_M_const
	{
		return TERARK_M_iterator(this, m_index.upper_bound(key, comp));
	}
	template<class CompatibleCompare>
	std::pair<TERARK_M_iterator, TERARK_M_iterator>
	equal_range(const key_type& key, CompatibleCompare comp) TERARK_M_const
	{
		std::pair<typename index_t::TERARK_M_iterator,
				  typename index_t::TERARK_M_iterator>
		ii = m_index.equal_range(key, comp);
		return std::pair<TERARK_M_iterator, TERARK_M_iterator>(
						 TERARK_M_iterator(this, ii.first),
						 TERARK_M_iterator(this, ii.second));
	}

	//////////////////////////////////////////////////////////////////////////

	TERARK_M_iterator lower_bound(const key_type& key) TERARK_M_const
	{
		return lower_bound(key, comp());
	}
	TERARK_M_iterator upper_bound(const key_type& key) TERARK_M_const
	{
		return upper_bound(key, comp());
	}
	std::pair<TERARK_M_iterator, TERARK_M_iterator>
		equal_range(const key_type& key) TERARK_M_const
	{
		return equal_range(key, comp());
	}

	///////////////////////////////////////////////////////////////////////////////////////////////

	std::ptrdiff_t distance(const TERARK_M_iterator& iter1,
							const TERARK_M_iterator& iter2) TERARK_M_const
	{
		return iter1.distance(iter2);
	}

	size_type equal_count(const TERARK_M_iterator& iter) TERARK_M_const
	{
		return iter.equal_count();
	}

	/**
	 @{
	 @brief return equal_count and goto next larger
	 */
	size_type goto_next_larger(TERARK_M_iterator& iter) TERARK_M_const
	{
		return iter.goto_next_larger();
	}
	//@}

	bool equal_next(const TERARK_M_iterator& iter) TERARK_M_const
	{
		return iter.equal_next();
	}

	TERARK_M_iterator next_larger(const TERARK_M_iterator& iter) TERARK_M_const
	{
		TERARK_M_iterator next(iter); next.goto_next_larger();
		return next;
	}

	///////////////////////////////////////////////////////////////////////////////////////////////

	template<class ValueCompare>
	TERARK_M_iterator
	value_lower_bound(TERARK_M_iterator ilow, TERARK_M_iterator iupp,
					  const value_type& val, ValueCompare vcomp) TERARK_M_const
	{
		assert(ilow.m_iter + 1 == iupp.m_iter);

		typename std::vector<value_type>::TERARK_M_iterator
		iter = std::lower_bound(
			m_values.begin() + *ilow.m_iter,
			m_values.begin() + *iupp.m_iter,
			val, vcomp
			);
		if (m_values.begin() + *iupp.m_iter == iter)
			return TERARK_M_iterator(this, iupp.m_iter, iter - m_values.begin());
		else
			return TERARK_M_iterator(this, ilow.m_iter, iter - m_values.begin());
	}

	template<class ValueCompare>
	TERARK_M_iterator
	value_upper_bound(TERARK_M_iterator ilow, TERARK_M_iterator iupp,
					  const value_type& val, ValueCompare vcomp) TERARK_M_const
	{
		assert(ilow.m_iter + 1 == iupp.m_iter);

		typename std::vector<value_type>::TERARK_M_iterator
		iter = std::upper_bound(
			m_values.begin() + *ilow.m_iter,
			m_values.begin() + *iupp.m_iter,
			val, vcomp
			);
		if (m_values.begin() + *iupp.m_iter == iter)
			return TERARK_M_iterator(this, iupp.m_iter, iter - m_values.begin());
		else
			return TERARK_M_iterator(this, ilow.m_iter, iter - m_values.begin());
	}

	template<class ValueCompare>
	std::pair<TERARK_M_iterator, TERARK_M_iterator>
	value_equal_range(TERARK_M_iterator ilow, TERARK_M_iterator iupp,
					  const value_type& val, ValueCompare vcomp) TERARK_M_const
	{
		assert(ilow.m_iter + 1 == iupp.m_iter);

		std::pair<typename std::vector<value_type>::TERARK_M_iterator,
				  typename std::vector<value_type>::TERARK_M_iterator> ii =
		std::equal_range(
			m_values.begin() + *ilow.m_iter,
			m_values.begin() + *iupp.m_iter,
			val, vcomp
			);
		typename index_t::TERARK_M_iterator jj1 = ilow.m_iter, jj2 = iupp.m_iter;

		if (m_values.begin() + *iupp.m_iter == ii.first)
			++jj1;
		if (m_values.begin() + *iupp.m_iter == ii.second)
			++jj2;
		return std::pair<TERARK_M_iterator, TERARK_M_iterator>(
						 TERARK_M_iterator(this, jj1, ii.first  - m_values.begin()),
						 TERARK_M_iterator(this, jj2, ii.second - m_values.begin()));
	}

	std::ptrdiff_t value_distance(const TERARK_M_iterator& iter1,
								  const TERARK_M_iterator& iter2) TERARK_M_const
	{
		return iter2.m_val_index - iter1.m_val_index;
	}


#undef TERARK_M_reverse_iterator
#undef TERARK_M_iterator
#undef TERARK_M_const
