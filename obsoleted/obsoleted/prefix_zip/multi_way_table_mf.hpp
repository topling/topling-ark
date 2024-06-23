/* vim: set tabstop=4 : */

	template<class T1, class T2>
	int compare(const T1& t1, const T2& t2) TERARK_M_const
	{
		return EffectiveCompare(m_comp, t1, t2);
	}

	TERARK_M_iterator begin() TERARK_M_const
	{
		return TERARK_M_iterator(this, boost::mpl::true_());
	}

	TERARK_M_iterator end() TERARK_M_const
	{
		return TERARK_M_iterator(this, boost::mpl::false_());
	}

	TERARK_M_iterator rbegin() TERARK_M_const
	{
		return TERARK_M_reverse_iterator(this, boost::mpl::true_());
	}

	TERARK_M_iterator rend() TERARK_M_const
	{
		return TERARK_M_reverse_iterator(this, boost::mpl::false_());
	}

	/**
	 @name search functions
	 @{
	 -# 可传入一个可选的 compare
	 -# 如果是查找相同前缀的的范围，xkey 必须就刚好是那个前缀，不能包含比前缀更多字符！并且 comp 必须是前缀比较器
	 */
	TERARK_M_iterator lower_bound(const key_type& xkey) TERARK_M_const
	{
		return lower_bound(xkey, m_comp);
	}
	TERARK_M_iterator upper_bound(const key_type& xkey) TERARK_M_const
	{
		return upper_bound(xkey, m_comp);
	}
	std::pair<TERARK_M_iterator, TERARK_M_iterator>
		equal_range(const key_type& xkey) TERARK_M_const
	{
		return equal_range(xkey, m_comp);
	}

	template<class CompatibleCompare>
	TERARK_M_iterator lower_bound(const key_type& xkey, CompatibleCompare comp) TERARK_M_const
	{
		TERARK_M_iterator iter(this);

		for (int i = 0; i < m_fixed.size(); ++i)
		{
			typename fixed_index::TERARK_M_iterator found = m_fixed[i].lower_bound(xkey, comp);
			typename fixed_index::TERARK_M_iterator iend  = m_fixed[i].end();
			if (found != iend)
				iter.m_fvec.push_back(typename TERARK_M_iterator::FixedIndexElem(this, i, found, iend));
		}
		iter.m_iinc = m_inc.lower_bound(xkey, comp);
		iter.sort();
		return iter;
	}

	template<class CompatibleCompare>
	TERARK_M_iterator upper_bound(const key_type& xkey, CompatibleCompare comp) TERARK_M_const
	{
		TERARK_M_iterator iter(this);

		for (int i = 0; i < m_fixed.size(); ++i)
		{
			typename fixed_index::TERARK_M_iterator found = m_fixed[i].upper_bound(xkey, comp);
			typename fixed_index::TERARK_M_iterator iend  = m_fixed[i].end();
			if (found != iend)
				iter.m_fvec.push_back(typename TERARK_M_iterator::FixedIndexElem(this, i, found, iend));
		}
		iter.m_iinc = m_inc.upper_bound(xkey, comp);
		iter.sort();
		return iter;
	}

	template<class CompatibleCompare>
	std::pair<TERARK_M_iterator, TERARK_M_iterator>
	equal_range(const key_type& xkey, CompatibleCompare comp) TERARK_M_const
	{
		TERARK_M_iterator iter1(this), iter2(this);

		for (int i = 0; i < m_fixed.size(); ++i)
		{
			std::pair<typename fixed_index::TERARK_M_iterator,
					  typename fixed_index::TERARK_M_iterator>
				ii = m_fixed[i].equal_range(xkey, comp);
			typename fixed_index::TERARK_M_iterator iend  = m_fixed[i].end();
			if (iend != ii.first)
				iter1.m_fvec.push_back(typename TERARK_M_iterator::FixedIndexElem(this, i, ii.first , iend));
			if (iend != ii.second)
				iter2.m_fvec.push_back(typename TERARK_M_iterator::FixedIndexElem(this, i, ii.second, iend));
		}
		std::pair<typename increment_index::TERARK_M_iterator,
				  typename increment_index::TERARK_M_iterator>
		ii = m_inc.equal_range(xkey, comp);
		iter1.m_iinc = ii.first;
		iter2.m_iinc = ii.second;

		iter1.sort();
		iter2.sort();

		return std::make_pair(iter1, iter2);
	}

	//@}

	TERARK_M_iterator find(const key_type& xkey) TERARK_M_const
	{
		std::pair<TERARK_M_iterator, TERARK_M_iterator> ii = equal_range(xkey, m_comp);
		if (ii.first != ii.second)
			return ii.first;
		else
			return this->end();
	}

	size_type equal_count(const TERARK_M_iterator& iter) TERARK_M_const
	{
		return iter.equal_count();
	}

	bool equal_next(const TERARK_M_iterator& iter) TERARK_M_const
	{
		return iter.equal_next();
	}

	size_type goto_next_larger(TERARK_M_iterator& iter) TERARK_M_const
	{
		return iter.goto_next_larger();
	}

	ptrdiff_t distance(const TERARK_M_iterator& lesser,
					   const TERARK_M_iterator& larger) TERARK_M_const
	{
		return lesser.distance(larger);
	}

//////////////////////////////////////////////////////////////////////////
//! lower_bound/upper_bound
#define TERARK_MULTI_WAY_TABLE_KEY_OR_VAL	key_type

#define TERARK_MULTI_WAY_TABLE_FUNC	lower_bound
#include "multi_way_table_mf_bound.hpp"

#define TERARK_MULTI_WAY_TABLE_FUNC	upper_bound
#include "multi_way_table_mf_bound.hpp"

#define TERARK_MULTI_WAY_TABLE_FUNC	equal_range
#include "multi_way_table_mf_range.hpp"

#undef TERARK_MULTI_WAY_TABLE_KEY_OR_VAL
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//! value_lower_bound/value_upper_bound
#define TERARK_MULTI_WAY_TABLE_VALUE
#define TERARK_MULTI_WAY_TABLE_KEY_OR_VAL	value_type

#define TERARK_MULTI_WAY_TABLE_FUNC	value_lower_bound
#include "multi_way_table_mf_bound.hpp"

#define TERARK_MULTI_WAY_TABLE_FUNC	value_upper_bound
#include "multi_way_table_mf_bound.hpp"

#define TERARK_MULTI_WAY_TABLE_FUNC	value_equal_range
#include "multi_way_table_mf_range.hpp"

#undef TERARK_MULTI_WAY_TABLE_KEY_OR_VAL
#undef TERARK_MULTI_WAY_TABLE_VALUE
//////////////////////////////////////////////////////////////////////////

	ptrdiff_t value_distance(const TERARK_M_iterator& iter1,
							 const TERARK_M_iterator& iter2) TERARK_M_const
	{
		TERARK_M_iterator iter11(iter1), iter22(iter2);

		iter11.sort_index();
		iter22.sort_index();

		ptrdiff_t dist = 0;
		int i = 0, j = 0;
		for (; i != iter11.size() && j != iter22.size(); )
		{
			if (iter11.m_fvec[i].index == iter22.m_fvec[j].index)
			{
				dist += m_fixed[iter11.m_fvec[i].index].
						value_distance(iter11.m_fvec[i].iter, iter22.m_fvec[j].iter);
				++i; ++j;
			}
			else if (iter11.m_fvec[i].index < iter22.m_fvec[j].index)
			{
				++i;
			}
			else
			{
				// 如果程序正确，不会走到这里
				assert(0);
			}
		}
		dist += m_inc.value_distance(iter11.m_iinc, iter22.m_iinc);
		return dist;
	}


#undef TERARK_M_reverse_iterator
#undef TERARK_M_iterator
#undef TERARK_M_const
