/* vim: set tabstop=4 : */
//***************************************************************************************
//! @file used for generate iterators:
//! @{
//! -# MultiWayTable_Impl_iterator, when TERARK_M_iterator=iterator
//! -# MultiWayTable_Impl_const_iterator, when TERARK_M_iterator=const_iterator
//! -# MultiWayTable_Impl_reverse_iterator, when TERARK_M_iterator=reverse_iterator
//! -# MultiWayTable_Impl_const_reverse_iterator, when TERARK_M_iterator=const_reverse_iterator
//!
//! param TERARK_M_iterator
//! param TERARK_M_const
//!
//! param TERARK_iter_beg
//! param TERARK_iter_end
//! param TERARK_MWT_ITER_compare_t

//! param TERARK_MWT_ITER_is_const_iterator, define when generate const_iterator/const_reverse_iterator
//! param TERARK_MWT_ITER_is_reverse_iterator, define when generate reverse_iterator/const_reverse_iterator
//@}

#define MultiWayTable_Iterator BOOST_JOIN(MultiWayTable_Impl_, TERARK_M_iterator)

template<class BaseClassT>
class MultiWayTable_Iterator
	: public boost::forward_iterator_helper<
			MultiWayTable_Iterator<BaseClassT>,
			typename BaseClassT::value_type,
			ptrdiff_t,
			TERARK_M_const typename BaseClassT::value_type*,
			TERARK_M_const typename BaseClassT::value_type&
		>
{
	friend class MultiWayTable_Impl<BaseClassT>;

	typedef MultiWayTable_Impl<BaseClassT> owner_t;

	typedef typename owner_t::fixed_index fixed_index;
	typedef typename fixed_index::TERARK_M_iterator f_iter_t;

	typedef typename owner_t::increment_index increment_index;
	typedef typename increment_index::TERARK_M_iterator i_iter_t;

public:
	typedef typename BaseClassT::key_type	key_type;
	typedef typename BaseClassT::value_type	value_type;
	typedef TERARK_M_const value_type&		reference;
	typedef TERARK_MWT_ITER_compare_t		compare_t;

protected:
	struct FixedIndexElem
	{
		int       indx;
		f_iter_t  iter;
		f_iter_t  iend;
		key_type  x_kv;

		template<class OwnerPtrT>
		FixedIndexElem(OwnerPtrT owner, int index, f_iter_t iter, f_iter_t iend)
			: indx(index)
			, iter(iter)
			, iend(iend)
		{
			assert(iter != iend);
			MULTI_WAY_EXTRACT_KV(owner->fixed()[indx], iter, x_kv);
		}

		template<class FixedIndexElem2>
		FixedIndexElem(const FixedIndexElem2& other)
			: indx(other.indx)
			, iter(other.iter)
			, iend(other.iend)
			, x_kv(other.x_kv)
		{
			assert(iter != iend);
		}

		template<class OwnerPtrT>
		void copyKey(OwnerPtrT owner)
		{
			assert(iter != iend);
			MULTI_WAY_EXTRACT_KV(owner->fixed()[indx], iter, x_kv);
		}

		bool is_end() const { return iter == iend; }

		bool operator==(const FixedIndexElem& right) const
		{
			return indx == right.indx && iter == right.iter;
		}
		bool operator!=(const FixedIndexElem& right) const
		{
			return !(*this == right);
		}
		friend void swap(FixedIndexElem& left, FixedIndexElem& right)
		{
			std::swap(left.indx, right.indx);
			std::swap(left.iter, right.iter);
			std::swap(left.iend, right.iend);
			std::swap(left.x_kv, right.x_kv);
		}
	};
// 	class CompareElem_Guard_End : private compare_t
// 	{
// 	public:
// 		CompareElem_Guard_End(compare_t comp) : compare_t(comp) {}
//
// 		bool operator()(const FixedIndexElem& left, const FixedIndexElem& right) const
// 		{
// 			if (left.iter == left.iend)
// 			{
// 				if (right.iter == right.iend)
// 					return false; // equal, must return false
// 				else
// 					// if not reverse, here left > right, should return false
// 					// but heap should reverse compare, return true
// 					// left comes end, left >= right, !(left < right)
// 					return true;
// 			}
// 			else if (right.iter == right.iend)
// 				return false;
// 			else
// 				// reverse compare left, right
// 				// right < left, left > right
// 				return compare_t::operator()(right.x_kv, left.x_kv);
// 		}
// 	};
	class CompareElem_InverseOnly : private compare_t
	{
	public:
		CompareElem_InverseOnly(compare_t comp)
			: compare_t(comp) {}

		bool operator()(const FixedIndexElem& left, const FixedIndexElem& right) const
		{
			assert(left .iter != left .iend);
			assert(right.iter != right.iend);
			return compare_t::operator()(right.x_kv, left.x_kv);
		}
	};
	struct CompareIndex
	{
		bool operator()(const FixedIndexElem& x, const FixedIndexElem& y) const
		{
			return x.indx < y.indx;
		}
	};

	typedef std::vector<FixedIndexElem> fvec_t;

	TERARK_M_const owner_t* m_owner;

	//! define: if *this == m_owner->end(), let m_is_in_fixed = false;
	bool m_is_in_fixed;

// 	typedef multi_way::HeapMultiWay<
// 		fvec_t::iterator,
// 		key_type,
// 		true,
// 		compare_t,
// 		GetKeyFromIter // 这个 Functor 没法写，因为 segment 的 key 是 iter->key()
// 	>
// 	fixed_multi_way;

	fvec_t   m_fvec;
	i_iter_t m_iinc;

	CompareElem_InverseOnly compHeap()       { return CompareElem_InverseOnly(m_owner->comp()); }
	CompareElem_InverseOnly compHeap() const { return CompareElem_InverseOnly(m_owner->comp()); }

	bool bcomp(const key_type& x, const key_type& y) const
	{
		return m_owner->comp()(x, y);
	}

	void sort()
	{
		if (m_fvec.empty()) {
			m_is_in_fixed = false;
			return;
		}
		std::make_heap(m_fvec.begin(), m_fvec.end(), compHeap());
		if (m_owner->incr().TERARK_iter_end() == m_iinc) {
			// @see this->is_end()
		//	m_is_in_fixed = !m_fvec[0].is_end(m_owner);
			m_is_in_fixed = true;
		}
		else if (m_fvec[0].is_end()) {
			// @see this->is_end()
			assert(m_owner->incr().TERARK_iter_end() != m_iinc);
			m_is_in_fixed = false;
		}
		else {
			// m_is_in_fixed = (fixed_index <= increment_index)
			m_is_in_fixed = !bcomp(m_owner->incr().raw_key(m_iinc), m_fvec[0].x_kv);
		}
	}

	void sort_index()
	{
		std::sort(m_fvec.begin(), m_fvec.end(), CompareIndex());
	}

protected:
	MultiWayTable_Iterator(TERARK_M_const owner_t* owner, boost::mpl::true_ asBegin)
	{
		m_owner = owner;
		for (int i = 0; i < owner->fixed().size(); ++i)
		{
			f_iter_t ibeg = owner->fixed()[i].TERARK_iter_beg();
			f_iter_t iend = owner->fixed()[i].TERARK_iter_end();
			if (ibeg != iend)
				m_fvec.push_back(FixedIndexElem(owner, i, ibeg, iend));
		}
		m_iinc = owner->incr().TERARK_iter_beg();
		sort();
	}
	MultiWayTable_Iterator(TERARK_M_const owner_t* owner, boost::mpl::false_ asBegin)
	{
		m_owner = owner;
		m_iinc = owner->incr().TERARK_iter_end();
		m_is_in_fixed = false; // end().m_is_in_fixed == false
	}

	MultiWayTable_Iterator(TERARK_M_const owner_t* owner)
	{
		m_owner = owner;
	}

public:
	MultiWayTable_Iterator()
	{
		m_owner = 0;
	}

#ifdef TERARK_MWT_ITER_is_reverse_iterator
#define	TERARK_MWT_ITER_to_reverse() convert_to_reverse_iter()
#else
#define	TERARK_MWT_ITER_to_reverse()
#endif

#define TERARK_MWT_construct(SrcIter)						\
	MultiWayTable_Iterator(const SrcIter<BaseClassT>& iter)	\
	 : m_owner(iter.m_owner)								\
	 , m_is_in_fixed(iter.m_is_in_fixed)					\
	 , m_fvec(iter.m_fvec.begin(), iter.m_fvec.end())		\
	 , m_iinc(iter.m_iinc)									\
	{														\
		TERARK_MWT_ITER_to_reverse();						\
	}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#ifdef TERARK_MWT_ITER_is_reverse_iterator
# ifdef TERARK_MWT_ITER_is_const_iterator
	TERARK_MWT_construct(MultiWayTable_Impl_reverse_iterator);
	TERARK_MWT_construct(MultiWayTable_Impl_const_iterator);
	TERARK_MWT_construct(MultiWayTable_Impl_iterator);
# else
	friend class MultiWayTable_Impl_const_reverse_iterator<BaseClassT>;
	TERARK_MWT_construct(MultiWayTable_Impl_iterator);
# endif
#else // TERARK_MWT_ITER_is_reverse_iterator
# ifdef TERARK_MWT_ITER_is_const_iterator
	TERARK_MWT_construct(MultiWayTable_Impl_iterator)
	friend class MultiWayTable_Impl_const_reverse_iterator<BaseClassT>;
# else
	friend class MultiWayTable_Impl_const_iterator<BaseClassT>;
	friend class MultiWayTable_Impl_reverse_iterator<BaseClassT>;
	friend class MultiWayTable_Impl_const_reverse_iterator<BaseClassT>;
# endif // TERARK_MWT_ITER_is_const_iterator
#endif  // TERARK_MWT_ITER_is_reverse_iterator

#undef TERARK_MWT_construct
#undef TERARK_MWT_ITER_to_reverse

	void swap(MultiWayTable_Iterator& rhs)
	{
		std::swap(m_is_in_fixed	, rhs.m_is_in_fixed);
		std::swap(m_owner	, rhs.m_owner);
		std::swap(m_fvec	, rhs.m_fvec);
		std::swap(m_iinc	, rhs.m_iinc);
	}

	key_type key() const
	{
		assert(m_owner);
		assert(!is_end());
		if (m_is_in_fixed)
			return m_fvec[0].x_kv;
		else
			return m_owner->incr().key(m_iinc);
	}

	reference operator*() const
	{
		assert(m_owner);
		assert(!is_end());

		if (m_is_in_fixed)
			return *m_fvec[0].iter;
		else {
			assert(!m_owner->incr().empty());
			assert(m_owner->incr().TERARK_iter_end() != m_iinc);
			return *m_iinc;
		}
	}

	bool is_end() const
	{
		bool bRet;

		if (m_owner->incr().TERARK_iter_end() != m_iinc)
			bRet = false;
		else {
		//	bRet = m_fvec.empty() || m_fvec.is_end();
			bRet = m_fvec.empty();

#if defined(_DEBUG) || !defined(NDEBUG)
			if (bRet)
			{
				// 如果是 end，m_is_in_fixed 必然为 false，这是设计
				assert(!m_is_in_fixed);
			}
			else
			{
				assert(!m_fvec[0].is_end());
			}
#endif
		}
		return bRet;
	}

protected:
	size_t equal_count_aux(boost:: true_type isUniqueKey) const { return 1; }
	size_t equal_count_aux(boost::mpl::false_ isUniqueKey) const
	{
		MultiWayTable_Iterator iter(*this);
		return iter.goto_next_larger();
	}

	size_t top_goto_next_larger()
	{
		FixedIndexElem& top = m_fvec.front();
		size_t  equal_count = m_owner->fixed()[top.indx].goto_next_larger(top.iter);
		return  equal_count;
	}

	void ajust_for_update_top()
	{
		assert(m_is_in_fixed);

		if (m_fvec.front().is_end()) {
			pop_heap_ignore_top(m_fvec.begin(), m_fvec.end(), compHeap());
			m_fvec.pop_back();
		} else {
			m_fvec.front().copyKey(m_owner);
			adjust_heap_top(m_fvec.begin(), m_fvec.end(), compHeap());
		}
	}
	void increment_fixed(boost::mpl::true_)
	{
		++m_fvec.front().iter;
		ajust_for_update_top();
		select_fixed_or_incr();
	}
	void increment_fixed(boost::mpl::false_)
	{
		assert(!m_fvec.empty());

		if (m_fvec[0].iter.equal_next())
		{
			++m_fvec[0].iter;

			// 如果这个断言失败，表明 fixed_index::equal_next 实现有问题
			assert(!m_fvec[0].is_end());
		}
		else {
			++m_fvec.front().iter;
			ajust_for_update_top();
			select_fixed_or_incr();
		}
	}

	void select_fixed_or_incr()
	{
		// 在固定索引和增量索引之间选择：
	//	if (!m_fvec.empty() && !m_fvec[0].is_end())
		if (!m_fvec.empty())
		{
			if (m_owner->incr().TERARK_iter_end() == m_iinc)
				m_is_in_fixed = true;
			else
				// m_is_in_fixed = (fixed_index <= increment_index)
				m_is_in_fixed = !bcomp(m_owner->incr().raw_key(m_iinc), m_fvec[0].x_kv);
		} else {
			// 固定索引为空，或者已达到末尾
			m_is_in_fixed = false; // 已经是 false, 不需要这个语句
		}
	}

	void increment_incr_index()
	{
		assert(!m_is_in_fixed);
		assert(!m_owner->incr().empty());
		assert(m_owner->incr().TERARK_iter_end() != m_iinc);

		++m_iinc;
		select_fixed_or_incr();
	}

	size_t goto_next_larger_aux(boost::mpl::true_ isUniqueKey)
	{
		++*this;
	//	printf("%s:%d, equal_count=%d\n", __FILE__, __LINE__, 1);
		return 1;
	}
	size_t goto_next_larger_aux(boost::mpl::false_ isUniqueKey)
	{
		std::ptrdiff_t equal_count = 0;
		key_type xkey = this->key();

		if (m_is_in_fixed)
		{
			equal_count += fixed_goto_next_larger(xkey);
		}
		if (m_owner->incr().TERARK_iter_end() != m_iinc &&
		//	m_owner->compare(xkey, m_owner->incr().raw_key(m_iinc)) == 0
			!bcomp(xkey, m_owner->incr().raw_key(m_iinc)))
		{
			equal_count += m_owner->incr().goto_next_larger(m_iinc);
		}
		select_fixed_or_incr();
		return equal_count;
	}

	//! only for internal use, xkey=this->key()
	//! pass xkey by param is only for efficiency
	size_t fixed_goto_next_larger(const key_type& xkey)
	{
		assert(!m_fvec.empty());

		size_t equal_count = top_goto_next_larger();
		ajust_for_update_top();
		while (!m_fvec.empty() && !bcomp(xkey, m_fvec[0].x_kv))
		{
			equal_count += top_goto_next_larger();
			ajust_for_update_top();
		}
		return equal_count;
	}

	template<class ResultContainer>
	void query_result_impl(ResultContainer& result)
	{
		key_type xkey = this->key();

		if (m_is_in_fixed)
		{
			assert(!m_fvec.empty());

			m_owner->fixed()[m_fvec[0].indx].query_result(m_fvec[0].iter, result);
			top_goto_next_larger();
			ajust_for_update_top();

			while (!m_fvec.empty() && !bcomp(xkey, m_fvec[0].x_kv))
			{
				m_owner->fixed()[m_fvec[0].indx].query_result(m_fvec[0].iter, result);
				top_goto_next_larger();
				ajust_for_update_top();
			}
		}
		if (m_owner->incr().TERARK_iter_end() != m_iinc &&
			(!m_is_in_fixed || !bcomp(xkey, m_owner->incr().raw_key(m_iinc))))
		{
			result.insert(result.end(), m_iinc, m_owner->incr().next_larger(m_iinc));
		}
	}
	template<class ResultContainer>
	void query_result(ResultContainer& result) const
	{
		MultiWayTable_Iterator iter(*this);
		iter.query_result_impl(result);
	}
public:
	MultiWayTable_Iterator& operator++()
	{
		assert(m_owner);
		assert(!m_owner->empty());
		assert(!is_end());

		if (m_is_in_fixed)
			increment_fixed(typename owner_t::is_unique_key_t());
		else
			increment_incr_index();

		return static_cast<MultiWayTable_Iterator&>(*this);
	}

	// this->key() must less than larger.key()
	std::ptrdiff_t distance(const MultiWayTable_Iterator& larger) const
	{
		assert(bcomp(this->key(), larger.key()));

		MultiWayTable_Iterator iter(static_cast<const MultiWayTable_Iterator&>(*this));

		std::ptrdiff_t dist = 0;

		while (iter != larger)
		{
			dist += iter.goto_next_larger();
		}
		return dist;
	}

	size_t equal_count() const
	{
		assert(!is_end());
		return equal_count_aux(typename owner_t::is_unique_key_t());
	}
	size_t goto_next_larger()
	{
		assert(!is_end());
		return goto_next_larger_aux(typename owner_t::is_unique_key_t());
	}

	size_t goto_next_larger_no_sort_no_copy_key()
	{
		assert(!is_end());

		size_t count = 0;
		if (m_is_in_fixed)
		{
			while (!m_fvec.empty())
			{
				if (!bcomp(xkey, m_fvec[i].x_kv))
				{
					count += m_owner->fixed()[m_fvec[i].indx].goto_next_larger(m_fvec[i].iter);
				}
			}
		}
		if (!m_owner->incr().empty())
		{
			if (m_owner->incr().TERARK_iter_end() != m_iinc &&
				!bcomp(xkey, m_owner->incr().raw_key(m_iinc))
				)
			{
				count += m_owner->incr().goto_next_larger(m_iinc);
			}
		}
		return count;
	}

	friend bool operator==(const MultiWayTable_Iterator& left, const MultiWayTable_Iterator& right)
	{
		assert(left.m_owner == right.m_owner);

		if (left.m_is_in_fixed != right.m_is_in_fixed)
		{
			assert(left.m_iinc != right.m_iinc ||
				   left.m_fvec.size() != right.m_fvec.size() ||
				   !left.m_fvec.empty() && left.m_fvec[0] != right.m_fvec[0]);
			return false;
		}
		bool bRet;
		if (left.m_is_in_fixed) {
			assert(!left.m_fvec.empty());
			bRet = left.m_fvec[0] == right.m_fvec[0];
			assert(!bRet || left.m_iinc == right.m_iinc);
		} else {
			bRet = left.m_iinc == right.m_iinc;
#if defined(_DEBUG) || !defined(NDEBUG)
			if (bRet)
			{
				if (!left.m_fvec.empty() && !right.m_fvec.empty())
				{
					if (left.m_fvec[0] != right.m_fvec[0])
					{
						printf("m_fvec[0].indx[left=%d, right=%d]\n",
							left.m_fvec[0].indx, right.m_fvec[0].indx);
						assert(0);
					}
				}
			}
#endif
		}
		return bRet;
	}

protected:
	void convert_to_reverse_iter()
	{
		std::sort(m_fvec.begin(), m_fvec.end(), CompareIndex());
		fvec_t temp;
		int owner_fixed_index = 0;
		for (int fvec_index = 0; fvec_index != m_fvec.size();)
		{
			if (m_fvec[fvec_index].indx == owner_fixed_index)
			{
				// must change iend to fixed[i].rend()
				m_fvec[fvec_index].iend = m_owner->fixed()[fvec_index].TERARK_iter_end();

				if (!m_fvec[fvec_index].is_end())
					temp.push_back(m_fvec[fvec_index]);

				fvec_index++;
				owner_fixed_index++;
			}
			else // m_fvec[fvec_index] is iterator.end(), so it is reverse_iterator.rbegin()
			{
				f_iter_t ibeg = owner->fixed()[owner_fixed_index].TERARK_iter_beg();
				f_iter_t iend = owner->fixed()[owner_fixed_index].TERARK_iter_end();
				if (ibeg != iend)
					temp.push_back(FixedIndexElem(owner, owner_fixed_index, ibeg, iend));

				owner_fixed_index++;
			}
		}
		temp.swap(m_fvec);
		sort();
	}
};

#undef MultiWayTable_Iterator

#undef TERARK_M_reverse_iterator
#undef TERARK_M_iterator
#undef TERARK_M_const

