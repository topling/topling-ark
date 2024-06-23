/* vim: set tabstop=4 : */


	TERARK_M_iterator begin() TERARK_M_const { return TERARK_M_iterator(this, m_indexed.begin(), m_multiple.begin()); }
	TERARK_M_iterator end()   TERARK_M_const { return TERARK_M_iterator(this, m_indexed.end(),   m_multiple.end()); }

	void key(TERARK_M_iterator iter, key_type& xkey) TERARK_M_const
	{
		if (iter.m_is_in_indexed)
			m_indexed.key(iter.m_indexed_iter, xkey);
		else
			m_multiple.key(iter.m_multiple_iter, xkey);
	}
	key_type key(TERARK_M_iterator iter) TERARK_M_const
	{
		key_type xkey;
		key(iter, xkey);
		return xkey;
	}

	raw_key_t raw_key(TERARK_M_iterator iter) TERARK_M_const
	{
		if (iter.m_is_in_indexed)
			return m_indexed.raw_key(iter.m_indexed_iter);
		else
			return m_multiple.raw_key(iter.m_multiple_iter);
	}

	/**
	 @brief 将 iterator 转化成 handle

	 @note 如非必要，不要使用该函数，该函数的时间开销相当于一次查找
	 */
	TERARK_M_iterator to_iterator(handle_t handle) TERARK_M_const
	{
		TERARK_M_iterator iter;
		if (handle.t & 0x80000000)
		{
			// is_in_indexed
			handle.t &= 0x7FFFFFFF;
			iter.m_is_in_indexed = true;
			iter.m_indexed_iter = begin().m_indexed_iter + handle.t;
			iter.m_multiple_iter = m_multiple.upper_bound(m_indexed.key(iter.m_indexed_iter));
		}
		else
		{
			iter.m_is_in_indexed = false;
			iter.m_multiple_iter = begin().m_multiple_iter + handle.t;
			iter.m_indexed_iter = m_indexed.upper_bound(owner->m_multiple.key(iter.m_multiple_iter));
		}
#ifdef TERARK_IS_IN_MULTI_ZSTRING_TABLE_OLD
		iter.m_val_index = *iter.m_indexed_iter;
#endif
		return iter;
	}

	/**
	 @brief 将 iterator 转化成 handle

	 @note 如非必要，不要使用该函数
	 */
	handle_t to_handle(const TERARK_M_iterator& iter) TERARK_M_const
	{
		assert(end() != iter);
		if (iter.m_is_in_indexed)
			return handle_t(0x80000000 | (iter.m_indexed_iter - begin().m_indexed_iter));
		else
			return handle_t(iter.m_multiple_iter - begin().m_multiple_iter);
	}

	template<class CompatibleCompare>
	TERARK_M_iterator
		lower_bound(TERARK_M_iterator first, TERARK_M_iterator last,
					const key_type& xkey, CompatibleCompare comp) TERARK_M_const
	{
		typename TERARK_M_iterator::indexed_iter_t u = m_indexed.lower_bound(
			first.m_indexed_iter, last.m_indexed_iter, xkey, comp);
		typename TERARK_M_iterator::multiple_iter_t n = m_multiple.lower_bound(
			first.m_multiple_iter, last.m_multiple_iter, xkey, comp);
		return TERARK_M_iterator(this, u, n);
	}
	template<class CompatibleCompare>
	TERARK_M_iterator
		upper_bound(TERARK_M_iterator first, TERARK_M_iterator last,
					const key_type& xkey, CompatibleCompare comp) TERARK_M_const
	{
		typename TERARK_M_iterator::indexed_iter_t u = m_indexed.upper_bound(
			first.m_indexed_iter, last.m_indexed_iter, xkey, comp);
		typename TERARK_M_iterator::multiple_iter_t n = m_multiple.upper_bound(
			first.m_multiple_iter, last.m_multiple_iter, xkey, comp);
		return TERARK_M_iterator(this, u, n);
	}
	template<class CompatibleCompare>
	std::pair<TERARK_M_iterator, TERARK_M_iterator>
		equal_range(TERARK_M_iterator first, TERARK_M_iterator last,
					const key_type& xkey, CompatibleCompare comp) TERARK_M_const
	{
		std::pair<typename TERARK_M_iterator::indexed_iter_t,
				  typename TERARK_M_iterator::indexed_iter_t>
			u = m_indexed.equal_range(first.m_indexed_iter, last.m_indexed_iter, xkey, comp);
		std::pair<typename TERARK_M_iterator::multiple_iter_t,
				  typename TERARK_M_iterator::multiple_iter_t>
			n = m_multiple.equal_range(first.m_multiple_iter, last.m_multiple_iter, xkey, comp);
		return std::pair<TERARK_M_iterator, TERARK_M_iterator>(
			TERARK_M_iterator(this, u.first, n.first),
			TERARK_M_iterator(this, u.second, n.second)
			);
	}

	//////////////////////////////////////////////////////////////////////////
	template<class CompatibleCompare>
	TERARK_M_iterator
		lower_bound(const key_type& xkey, CompatibleCompare comp) TERARK_M_const
	{
		typename TERARK_M_iterator::indexed_iter_t u = m_indexed.lower_bound(xkey, comp);
		typename TERARK_M_iterator::multiple_iter_t n = m_multiple.lower_bound(xkey, comp);
		return TERARK_M_iterator(this, u, n);
	}
	template<class CompatibleCompare>
	TERARK_M_iterator
		upper_bound(const key_type& xkey, CompatibleCompare comp) TERARK_M_const
	{
		typename TERARK_M_iterator::indexed_iter_t u = m_indexed.upper_bound(xkey, comp);
		typename TERARK_M_iterator::multiple_iter_t n = m_multiple.upper_bound(xkey, comp);
		return TERARK_M_iterator(this, u, n);
	}
	template<class CompatibleCompare>
	std::pair<TERARK_M_iterator, TERARK_M_iterator>
		equal_range(const key_type& xkey, CompatibleCompare comp) TERARK_M_const
	{
		std::pair<typename TERARK_M_iterator::indexed_iter_t,
				  typename TERARK_M_iterator::indexed_iter_t>
			u = m_indexed.equal_range(xkey, comp);

		std::pair<typename TERARK_M_iterator::multiple_iter_t,
				  typename TERARK_M_iterator::multiple_iter_t>
			n = m_multiple.equal_range(xkey, comp);

		return std::pair<TERARK_M_iterator, TERARK_M_iterator>(
			TERARK_M_iterator(this, u.first, n.first),
			TERARK_M_iterator(this, u.second, n.second)
			);
	}

	TERARK_M_iterator lower_bound(const key_type& xkey) TERARK_M_const
	{
		return lower_bound(xkey, comp());
	}
	TERARK_M_iterator upper_bound(const key_type& xkey) TERARK_M_const
	{
		return upper_bound(xkey, comp());
	}
	std::pair<TERARK_M_iterator, TERARK_M_iterator>
		equal_range(const key_type& xkey) TERARK_M_const
	{
		return equal_range(xkey, comp());
	}

	ptrdiff_t distance(const TERARK_M_iterator& lesser, const TERARK_M_iterator& larger) TERARK_M_const
	{
		return lesser.distance(larger);
	}

	size_type equal_count(const TERARK_M_iterator& iter) TERARK_M_const { return iter.equal_count(); }

	bool equal_next(const TERARK_M_iterator& iter) TERARK_M_const { return iter.equal_next(); }

	/**
	 @brief return equal_count and goto next larger
	 */
	size_type goto_next_larger(TERARK_M_iterator& iter) TERARK_M_const
	{
		return iter.goto_next_larger();
	}

	TERARK_M_iterator next_larger(const TERARK_M_iterator& iter) TERARK_M_const
	{
		TERARK_M_iterator next(iter); next.goto_next_larger();
		return next;
	}
#ifndef TERARK_IS_IN_MULTI_ZSTRING_TABLE_OLD
	template<class ValueCompare>
	TERARK_M_iterator
		value_lower_bound(TERARK_M_iterator ilow, TERARK_M_iterator iupp,
						  const value_type& val, ValueCompare vcomp) TERARK_M_const
	{
		assert(iupp.is_end() || (!comp()(key(iupp), key(ilow))));

		if (ilow.m_is_in_indexed)
		{
			return TERARK_M_iterator(this,
				m_indexed.value_lower_bound(ilow.m_indexed_iter, iupp.m_indexed_iter, val, vcomp),
				iupp.m_multiple_iter);
		}
		else
		{
			return TERARK_M_iterator(this,	iupp.m_indexed_iter,
				m_multiple.value_lower_bound(ilow.m_multiple_iter, iupp.m_multiple_iter, val, vcomp));
		}
	}
	template<class ValueCompare>
	TERARK_M_iterator
		value_upper_bound(TERARK_M_iterator ilow, TERARK_M_iterator iupp,
						  const value_type& val, ValueCompare vcomp) TERARK_M_const
	{
		assert(iupp.is_end() || (!comp()(key(iupp), key(ilow))));

		if (ilow.m_is_in_indexed)
		{
			return TERARK_M_iterator(this,
				m_indexed.value_upper_bound(ilow.m_indexed_iter, iupp.m_indexed_iter, val, vcomp),
				iupp.m_multiple_iter);
		}
		else
		{
			return TERARK_M_iterator(this,	iupp.m_indexed_iter,
				m_multiple.value_upper_bound(ilow.m_multiple_iter, iupp.m_multiple_iter, val, vcomp));
		}
	}

	template<class ValueCompare>
	std::pair<TERARK_M_iterator, TERARK_M_iterator>
		value_equal_range(TERARK_M_iterator ilow, TERARK_M_iterator iupp,
						  const value_type& val, ValueCompare vcomp) TERARK_M_const
	{
		assert(iupp.is_end() || (!comp()(key(iupp), key(ilow))));

		if (ilow.m_is_in_indexed)
		{
			std::pair<typename indexed_table_t::TERARK_M_iterator,
					  typename indexed_table_t::TERARK_M_iterator> ri =
				m_indexed.value_equal_range(ilow.m_indexed_iter, iupp.m_indexed_iter, val, vcomp);

			return std::pair<TERARK_M_iterator, TERARK_M_iterator>(
							 TERARK_M_iterator(this, ri.first , iupp.m_multiple_iter),
							 TERARK_M_iterator(this, ri.second, iupp.m_multiple_iter));
		}
		else
		{
			std::pair<typename multiple_table_t::TERARK_M_iterator,
					  typename multiple_table_t::TERARK_M_iterator> rm =
				m_multiple.value_equal_range(ilow.m_multiple_iter, iupp.m_multiple_iter, val, vcomp);
			return std::pair<TERARK_M_iterator, TERARK_M_iterator>(
							 TERARK_M_iterator(this, ilow.m_indexed_iter, rm.first),
							 TERARK_M_iterator(this, ilow.m_indexed_iter, rm.second));
		}
	}

	ptrdiff_t value_distance(const TERARK_M_iterator& iter1, const TERARK_M_iterator& iter2) TERARK_M_const
	{
		if (iter1.m_is_in_indexed)
			return m_indexed.value_distance(iter1.m_indexed_iter, iter2.m_indexed_iter);
		else
			return m_multiple.value_distance(iter1.m_multiple_iter, iter2.m_multiple_iter);
	}

#undef TERARK_M_reverse_iterator
#undef TERARK_M_iterator
#undef TERARK_M_const

#endif


