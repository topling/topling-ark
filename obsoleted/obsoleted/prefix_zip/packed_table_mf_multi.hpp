/* vim: set tabstop=4 : */
// for const and non-const

#include "packed_table_mf_common.hpp"

	bool can_insert(const KeyT& xkey) TERARK_M_const { return m_index.can_insert(xkey); }

	size_type size() TERARK_M_const { return m_valCount; }
	size_type val_count() TERARK_M_const { return m_valCount; }
	size_type key_count() TERARK_M_const { return m_index.size(); }
//	size_type distinct_size() TERARK_M_const { return m_index.size(); }

	template<typename CompatibleKey>
	size_type count(const CompatibleKey& x) TERARK_M_const
	{
		typename index_t::TERARK_M_iterator found = m_index.find(x);
		if (m_index.end() != x)
			return (*found).size();
		else
			return 0;
	}

	template<typename CompatibleKey,typename CompatibleCompare>
	size_type count(const CompatibleKey& x,CompatibleCompare comp) TERARK_M_const
	{
		std::pair<typename index_t::TERARK_M_iterator,
				  typename index_t::TERARK_M_iterator> range = m_index.find(x, comp);
		size_type n = 0;
		for (typename index_t::TERARK_M_iterator iter = range.first; iter != range.second; ++iter)
			n += (*iter).size();
		return n;
	}

	template<class ValueCompare>
	TERARK_M_iterator value_lower_bound(
		TERARK_M_iterator ilow, TERARK_M_iterator iupp,
		const value_type& val, ValueCompare vcomp) TERARK_M_const
	{
#if defined(_DEBUG) || !defined(NDEBUG)
		typename index_t::TERARK_M_iterator next = ilow.m_iter; ++next;
		assert(next == iupp.iter);
#endif
		typename val_vec_t::TERARK_M_iterator iter =
			std::lower_bound((*ilow).begin(), (*ilow).end(), val, vcomp);

		return TERARK_M_iterator(ilow.m_iter, iter - (*ilow).begin());
	}

	template<class ValueCompare>
	TERARK_M_iterator value_upper_bound(
		TERARK_M_iterator ilow, TERARK_M_iterator iupp,
		const value_type& val, ValueCompare vcomp) TERARK_M_const
	{
#if defined(_DEBUG) || !defined(NDEBUG)
		typename index_t::TERARK_M_iterator next = ilow.m_iter; ++next;
		assert(next == iupp.iter);
#endif
		typename val_vec_t::TERARK_M_iterator iter =
			std::upper_bound((*ilow).begin(), (*ilow).end(), val, vcomp);

		return TERARK_M_iterator(ilow.m_iter, iter - (*ilow).begin());
	}

	template<class ValueCompare>
	TERARK_M_iterator value_equal_range(
		TERARK_M_iterator ilow, TERARK_M_iterator iupp,
		const value_type& val, ValueCompare vcomp) TERARK_M_const
	{
#if defined(_DEBUG) || !defined(NDEBUG)
		typename index_t::TERARK_M_iterator next = ilow.m_iter; ++next;
		assert(next == iupp.iter);
#endif
		std::pair<typename val_vec_t::TERARK_M_iterator,
				  typename val_vec_t::TERARK_M_iterator> range =
			std::equal_range((*ilow).begin(), (*ilow).end(), val, vcomp);

		return std::pair<TERARK_M_iterator, TERARK_M_iterator>(
				TERARK_M_iterator(ilow.m_iter, range.first  - (*ilow).begin()),
				TERARK_M_iterator(ilow.m_iter, range.second - (*ilow).begin())
				);
	}

	ptrdiff_t value_distance(TERARK_M_iterator iter1, TERARK_M_iterator iter2) TERARK_M_const
	{
		if (iter1.m_iter == iter2.m_iter)
		{
			return iter2.m_nth - iter1.m_nth;
		}
		else
		{
#if defined(_DEBUG) || !defined(NDEBUG)
			typename index_t::TERARK_M_iterator next(iter1.m_iter); ++next;
			assert(next == iter2.m_iter);
#endif
			return (*iter1.m_iter).size() - iter1.m_nth;
		}
	}

	size_type equal_count(TERARK_M_iterator iter) TERARK_M_const
	{
		return (*iter.m_iter).size();
	}

	size_type goto_next_larger(TERARK_M_iterator& iter) TERARK_M_const
	{
		size_type count = (*iter.m_iter).size();
		iter.m_nth = 0;
		++iter.m_iter;
		return count;
	}

	TERARK_M_iterator next_larger(TERARK_M_iterator iter) TERARK_M_const
	{
		goto_next_larger(iter);
		return iter;
	}

	uint32_t  used_pool_size() TERARK_M_const { return m_index.used_pool_size(); }
	uint32_t total_pool_size() TERARK_M_const { return m_index.total_pool_size(); }
	uint32_t available_pool_size() TERARK_M_const { return m_index.available_pool_size(); }

	uint32_t  used_mem_size() TERARK_M_const { return m_index. used_mem_size() + sizeof(ValueT) * m_valCount; }
	uint32_t total_mem_size() TERARK_M_const { return m_index.total_mem_size() + sizeof(ValueT) * m_valCount; }

	raw_key_t raw_key(TERARK_M_iterator iter) TERARK_M_const
	{
		return m_index.raw_key(iter.m_iter);
	}
	raw_key_t raw_key(TERARK_M_reverse_iterator riter) TERARK_M_const
	{
		TERARK_M_iterator iter = riter.base(); --iter;
		return raw_key(iter);
	}

#undef TERARK_M_reverse_iterator
#undef TERARK_M_iterator
#undef TERARK_M_const
