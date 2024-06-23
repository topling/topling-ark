/* vim: set tabstop=4 : */
// for const and non-const

#include "packed_table_mf_common.hpp"

	template<class ValueCompare>
	TERARK_M_iterator value_lower_bound(TERARK_M_iterator ilow, TERARK_M_iterator iupp, const value_type& val, ValueCompare vcomp) TERARK_M_const
	{
		for (; ilow != iupp; ++ilow)
			if (!comp()(*ilow, val))
				return ilow;
		return iupp;
	}

	template<class ValueCompare>
	TERARK_M_iterator value_upper_bound(TERARK_M_iterator ilow, TERARK_M_iterator iupp, const value_type& val, ValueCompare vcomp) TERARK_M_const
	{
		for (; ilow != iupp; ++ilow)
			if (!comp()(*ilow, val))
				for (; ilow != iupp; ++ilow)
					if (comp(val, *ilow))
						return ilow;
		return iupp;
	}

	template<class ValueCompare>
	TERARK_M_iterator value_equal_range(TERARK_M_iterator ilow, TERARK_M_iterator iupp, const value_type& val, ValueCompare vcomp) TERARK_M_const
	{
		TERARK_M_iterator iter = ilow = value_lower_bound(ilow, iupp, val, vcomp);
		for (; iter != iupp; ++iter)
			if (comp(val, *iter))
				break;
		return std::pair<TERARK_M_iterator, TERARK_M_iterator>(ilow, iter);
	}

	ptrdiff_t value_distance(TERARK_M_iterator iter1, TERARK_M_iterator iter2) TERARK_M_const
	{
		return std::distance(iter1.m_iter, iter2.m_iter);
	}

	raw_key_t raw_key(TERARK_M_iterator iter) TERARK_M_const
	{
		return m_key_pool.raw_key(*iter.m_iter);
	}
	raw_key_t raw_key(TERARK_M_reverse_iterator riter) TERARK_M_const
	{
		TERARK_M_iterator iter = riter.base(); --iter;
		return raw_key(iter);
	}

	size_type equal_count(TERARK_M_iterator iter) TERARK_M_const
	{
		size_t ncount = goto_next_larger(&iter);
		return ncount;
	}

	size_type goto_next_larger(TERARK_M_iterator& iter) TERARK_M_const
	{
		return goto_next_larger_aux(this, iter, is_unique_key_t());
	}

	TERARK_M_iterator next_larger(TERARK_M_iterator iter) TERARK_M_const
	{
		goto_next_larger_aux(this, iter, is_unique_key_t());
		return iter;
	}

	uint32_t  used_pool_size() TERARK_M_const { return m_key_pool.used_size(); }
	uint32_t total_pool_size() TERARK_M_const { return m_key_pool.total_size(); }
	uint32_t available_pool_size() TERARK_M_const { return m_key_pool.total_size() - m_key_pool.used_size(); }

	uint32_t  used_mem_size() TERARK_M_const { return  used_pool_size() + m_index.size() * (node_t::PACK_SIZE + 3 * sizeof(void*)); }
	uint32_t total_mem_size() TERARK_M_const { return total_pool_size() + m_index.size() * (node_t::PACK_SIZE + 3 * sizeof(void*)); }

#undef TERARK_M_reverse_iterator
#undef TERARK_M_iterator
#undef TERARK_M_const
