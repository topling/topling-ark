/* vim: set tabstop=4 : */

//! generate equal_range/value_equal_range

//! @note:
//! must define
//! - TERARK_MULTI_WAY_TABLE_FUNC
//!	- TERARK_MULTI_WAY_TABLE_KEY_OR_VAL
//! if generate TERARK_MULTI_WAY_TABLE_FUNC, must define:
//!	- TERARK_MULTI_WAY_TABLE_VALUE

	template<class ValueCompare>
	std::pair<TERARK_M_iterator, TERARK_M_iterator>
	TERARK_MULTI_WAY_TABLE_FUNC(
		const TERARK_M_iterator& ilow, const TERARK_M_iterator& iupp,
		const TERARK_MULTI_WAY_TABLE_KEY_OR_VAL& k_or_v, ValueCompare comp) TERARK_M_const
	{
		typedef typename TERARK_M_iterator::FixedIndexElem FixedIndexElem;

		std::pair<TERARK_M_iterator, TERARK_M_iterator> ii(TERARK_M_iterator(this), TERARK_M_iterator(this));
		TERARK_M_iterator ilow2(ilow), iupp2(iupp);
		ilow2.sort_index();
		iupp2.sort_index();

		int i = 0, j = 0;
		while (i < ilow.m_fvec.size())
		{
			typename fixed_index::TERARK_M_iterator fiend = m_fixed[ilow2[i].index].end();
			typename fixed_index::TERARK_M_iterator fiupp;
			const int index = ilow2.m_fvec[i].index;
			if (index == iupp2.m_fvec[j].index)
				fiupp = iupp2.m_fvec[j].iter;
			else
				fiupp = fiend;
			std::pair<typename fixed_index::TERARK_M_iterator,
					  typename fixed_index::TERARK_M_iterator>
			fii = m_fixed[index].TERARK_MULTI_WAY_TABLE_FUNC(ilow2.m_fvec[i].iter, fiupp, k_or_v, comp);
			if (fiend != fii.first)
				ii.first.m_fvec.push_back(FixedIndexElem(this, ilow2[i].index, fii.first, fiend));
			if (fiend != fii.second)
				ii.second.m_fvec.push_back(FixedIndexElem(this, ilow2[i].index, fii.second, fiend));
		}
		std::pair<typename increment_index::TERARK_M_iterator,
				  typename increment_index::TERARK_M_iterator> iii =
#ifdef TERARK_MULTI_WAY_TABLE_VALUE
		m_inc.TERARK_MULTI_WAY_TABLE_FUNC(ilow.m_inc, iupp.m_inc, k_or_v, comp);
#else
		m_inc.TERARK_MULTI_WAY_TABLE_FUNC(k_or_v, comp);
#endif
		ii.first.m_inc = iii.first;
		ii.second.m_inc = iii.second;
		ii.first.sort();
		ii.second.sort();

		return ii;
	}

#undef TERARK_MULTI_WAY_TABLE_FUNC

