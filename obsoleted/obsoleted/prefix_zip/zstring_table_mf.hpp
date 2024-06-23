/* vim: set tabstop=4 : */

//	CompareT comp() TERARK_M_const { return m_comp };

	template<class CompatibleCompare>
	TERARK_M_iterator
	lower_bound_impl(TERARK_M_iterator first, TERARK_M_iterator last,
					 const key_type& xkey, CompatibleCompare comp,
					 boost::mpl::true_ isUniqueKey) TERARK_M_const
	{
		difference_type len = last - first;
		std::string str; // ZString<3>::MAX_ZSTRING == 259, little enough, and effient
		str.reserve(MAX_ZSTRING < 259 ? MAX_ZSTRING : 259);

		while (len > 0)
		{
			difference_type half = len >> 1;
			TERARK_M_iterator middle = first + half;
			this->key(middle, str);

			if (comp(str, xkey)) {
				first = middle + 1;
				len = len - half - 1;
			} else
				len = half;
		}
		return first;
	}

	template<class CompatibleCompare>
	TERARK_M_iterator
	upper_bound_impl(TERARK_M_iterator first, TERARK_M_iterator last,
					 const key_type& xkey, CompatibleCompare comp,
					 boost::mpl::true_ isUniqueKey) TERARK_M_const
	{
		difference_type len = last - first;
		std::string str; // ZString<3>::MAX_ZSTRING == 259, little enough, and effient
		str.reserve(MAX_ZSTRING < 259 ? MAX_ZSTRING : 259);

		while (len > 0)
		{
			difference_type half = len >> 1;
			TERARK_M_iterator middle = first + half;
			this->key(middle, str);

			if (comp(xkey, str))
				len = half;
			else {
				first = middle + 1;
				len = len - half - 1;
			}
		}
		return first;
	}

	template<class CompatibleCompare>
	std::pair<TERARK_M_iterator, TERARK_M_iterator>
	equal_range_impl(TERARK_M_iterator first, TERARK_M_iterator last,
					 const key_type& xkey, CompatibleCompare comp,
					 boost::mpl::true_ isUniqueKey) TERARK_M_const
	{
		difference_type len = last - first;
		std::string str; // ZString<3>::MAX_ZSTRING == 259, little enough, and effient
		str.reserve(MAX_ZSTRING < 259 ? MAX_ZSTRING : 259);

		while (len > 0) {
			difference_type half = len >> 1;
			TERARK_M_iterator middle = first + half;
			this->key(middle, str);
			int x = EffectiveCompare(comp, this->key(middle), xkey);
			if (x < 0) {
				first = middle + 1;
				len = len - half - 1;
			}
			else if (x > 0)
				len = half;
			else {
				TERARK_M_iterator left  = lower_bound_impl(first, middle, xkey, comp, isUniqueKey);
				TERARK_M_iterator right = upper_bound_impl(++middle, first + len, xkey, comp, isUniqueKey);
				return std::pair<TERARK_M_iterator, TERARK_M_iterator>(left, right);
			}
		}
		return std::pair<TERARK_M_iterator, TERARK_M_iterator>(first, first);
	}

	// 经过测试，按优化编译选项，有 5000 个重复 key 的查找
	// boost::mpl::false_ isUniqueKey 的版本，效率的提高只有 10% 左右

	template<class CompatibleCompare>
	TERARK_M_iterator
	lower_bound_impl(TERARK_M_iterator first, TERARK_M_iterator last,
					 const key_type& xkey, CompatibleCompare comp,
					 boost::mpl::false_ isUniqueKey) TERARK_M_const
	{
		difference_type len = last - first;
		std::string str; // ZString<3>::MAX_ZSTRING == 259, little enough, and effient
		str.reserve(MAX_ZSTRING < 259 ? MAX_ZSTRING : 259);

		while (len > 0)
		{
			difference_type half = len >> 1;
			TERARK_M_iterator middle = first + half;
			this->key(middle, str);
			int x = EffectiveCompare(comp, str, xkey);

			if (x < 0) {
				first = middle + 1;
				len = len - half - 1;
			}
			else if (x > 0)
				len = half;
			else if ((*middle.m_iter).offset == (*first.m_iter).offset)
				return first;
			else
				len = half;
		}
		return first;
	}

	template<class CompatibleCompare>
	TERARK_M_iterator
	upper_bound_impl(TERARK_M_iterator first, TERARK_M_iterator last,
					 const key_type& xkey, CompatibleCompare comp,
					 boost::mpl::false_ isUniqueKey) TERARK_M_const
	{
		difference_type len = last - first;
		std::string str; // ZString<3>::MAX_ZSTRING == 259, little enough, and effient
		str.reserve(MAX_ZSTRING < 259 ? MAX_ZSTRING : 259);

		while (len > 0)
		{
			difference_type half = len >> 1;
			TERARK_M_iterator middle = first + half;
			this->key(middle, str);
			int x = EffectiveCompare(comp, xkey, str);

			if (x < 0)
				len = half;
			else if (x > 0)
			{
				first = middle + 1;
				len = len - half - 1;
			}
			else if ((*middle.m_iter).offset == (*first.m_iter).offset)
			{
				return std::upper_bound(middle.m_iter + 1, first.m_iter + len,
					*(first.m_iter), typename node_t::CompOffset());
			}
			else {
				first = middle + 1;
				len = len - half - 1;
			}
		}
		return first;
	}

	template<class CompatibleCompare>
	std::pair<TERARK_M_iterator, TERARK_M_iterator>
	equal_range_impl(TERARK_M_iterator first, TERARK_M_iterator last,
					 const key_type& xkey, CompatibleCompare comp,
					 boost::mpl::false_ isUniqueKey) TERARK_M_const
	{
		difference_type len = last - first;
		std::string str; // ZString<3>::MAX_ZSTRING == 259, little enough, and effient
		str.reserve(MAX_ZSTRING < 259 ? MAX_ZSTRING : 259);

		while (len > 0)
		{
			difference_type half = len >> 1;
			TERARK_M_iterator middle = first + half;
			this->key(middle, str);
			int x = EffectiveCompare(comp, str, xkey);
			if (x < 0) {
				first = middle + 1;
				len = len - half - 1;
			}
			else if (x > 0)
				len = half;
			else if ((*middle.m_iter).offset == (*first.m_iter).offset)
			{
				TERARK_M_iterator upper = std::upper_bound(middle.m_iter + 1,
					first.m_iter + len, *(middle.m_iter), typename node_t::CompOffset());
				return std::pair<TERARK_M_iterator, TERARK_M_iterator>(first, upper);
			}
			else {
				TERARK_M_iterator left  = lower_bound_impl(first, middle, xkey, comp, isUniqueKey);
				TERARK_M_iterator right = upper_bound_impl(++middle, first + len, xkey, comp, isUniqueKey);
				return std::pair<TERARK_M_iterator, TERARK_M_iterator>(left, right);
			}
		}
		return std::pair<TERARK_M_iterator, TERARK_M_iterator>(first, first);
	}

	//////////////////////////////////////////////////////////////////////////

// 	TERARK_M_iterator
// 	find(TERARK_M_iterator first, TERARK_M_iterator last, const std::string& xkey) TERARK_M_const
// 	{
// 		BOOST_STATIC_ASSERT(IsUniqueKey); // only used on Unique
//
// 		difference_type len = last - first;
// 		std::string str; // ZString<3>::MAX_ZSTRING == 259, little enough, and effient
// 		str.reserve(MAX_ZSTRING < 259 ? MAX_ZSTRING : 259);
//
// 		int icomp = -1;
// 		while (len > 0)
// 		{
// 			difference_type half = len >> 1;
// 			TERARK_M_iterator middle = first + half;
// 			this->key(middle, str);
//
// 			icomp = EffectiveCompare(m_comp, str, xkey);
// 			if (icomp < 0) {
// 				first = middle + 1;
// 				len = len - half - 1;
// 			} else
// 				len = half;
// 		}
// 		if (0 == icomp)
// 			return first;
//
// 		if (first != last)
// 		{
// 			this->key(first, str);
// 			if (!m_comp(xkey, str))
// 				return first;
// 		}
// 		return this->end();
// 	}

	bool equal_next(const TERARK_M_iterator& iter) TERARK_M_const { return iter.equal_next(); }

	size_type equal_count(TERARK_M_iterator iter) TERARK_M_const
	{
		// assert: iter 必须是一组重复 key 的第一个
		assert(iter == begin() || iter != end() && (*(iter.m_iter-1)).offset != (*iter.m_iter).offset);

		return std::distance(iter.m_iter, next_larger(iter).m_iter);
	}

	/**
	 @brief return equal_count and goto next larger
	 */
	size_t goto_next_larger(TERARK_M_iterator& iter) TERARK_M_const
	{
		TERARK_M_iterator larger = upper_bound_near(
			iter.m_iter, m_index.end()-1, *iter.m_iter, typename node_t::CompOffset()
			);
		size_t equal_count = larger.m_iter - iter.m_iter;
		iter = larger;
		return equal_count;
	}

	/**
	 @brief return first_equal

	  first_equal is the iterator which key equal to the key iter point to
	 */
	TERARK_M_iterator first_equal(TERARK_M_iterator iter) TERARK_M_const
	{
		assert(this->end() != iter);
		return lower_bound_near(m_index.begin(), iter.m_iter, *iter.m_iter, typename node_t::CompOffset());
	}

#undef TERARK_M_reverse_iterator
#undef TERARK_M_iterator
#undef TERARK_M_const

