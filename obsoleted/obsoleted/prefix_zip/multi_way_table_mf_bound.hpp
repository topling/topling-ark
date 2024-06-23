/* vim: set tabstop=4 : */

//! generate lower_bound/upper_bound//value_lower_bound/value_upper_bound

//! @note:
//! must define
//! - TERARK_MULTI_WAY_TABLE_FUNC
//!	- TERARK_MULTI_WAY_TABLE_KEY_OR_VAL
//! if generate value_lower_bound/value_upper_bound, must define:
//!	- TERARK_MULTI_WAY_TABLE_VALUE

	template<class X_Compare>
	TERARK_M_iterator TERARK_MULTI_WAY_TABLE_FUNC(
		const TERARK_M_iterator& ilow, const TERARK_M_iterator& iupp,
		const TERARK_MULTI_WAY_TABLE_KEY_OR_VAL& k_or_v, X_Compare comp) TERARK_M_const
	{
		typedef typename TERARK_M_iterator::FixedIndexElem FixedIndexElem;

		TERARK_M_iterator iter(this), ilow2(ilow), iupp2(iupp);
		ilow2.sort_index();
		iupp2.sort_index();
		int i = 0, j = 0;
		while (i < ilow2.m_fvec.size())
		{
			typename fixed_index::TERARK_M_iterator fiend = m_fixed[ilow2[i].index].end();
			typename fixed_index::TERARK_M_iterator filow;
			if (ilow2[i].index == iupp2[j].index)
			{
				filow = m_fixed[index].TERARK_MULTI_WAY_TABLE_FUNC(ilow2.m_fvec[i].iter, iupp2.m_fvec[j].iter, k_or_v, comp);
				iter.m_fvec.push_back(FixedIndexElem(this, ilow2[i].index, filow, fiend));
				++i, ++j;
			}
			else // iupp2 has not this segment
			{
				filow =	m_fixed[index].TERARK_MULTI_WAY_TABLE_FUNC(ilow.m_fvec[i].iter, fiend, k_or_v, comp);
				if (filow != fiend)
					iter.m_fvec.push_back(FixedIndexElem(this, ilow2[i].index, filow, fiend));
				++i;
			}
		}
#ifdef TERARK_MULTI_WAY_TABLE_VALUE
		iter.m_inc = m_inc.TERARK_MULTI_WAY_TABLE_FUNC(ilow.m_inc, iupp.m_inc, k_or_v, comp);
#else
		iter.m_inc = m_inc.TERARK_MULTI_WAY_TABLE_FUNC(k_or_v, comp);
#endif
		iter.sort();
		return iter;
	}

#undef TERARK_MULTI_WAY_TABLE_FUNC

