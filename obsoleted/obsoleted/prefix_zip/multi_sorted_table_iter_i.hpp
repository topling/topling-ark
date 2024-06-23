/* vim: set tabstop=4 : */
// file: multi_sorted_table_iter_i.hpp
//
// must included in class MultiSortedTable
// must define:
// -# TERARK_M_const
// -# TERARK_M_iterator
// define TERARK_MULTI_SORTED_TABLE_IS_CONST_ITER when generate const_iterator

#define MultiSortedTable_M_Iterator BOOST_JOIN(MultiSortedTable_, TERARK_M_iterator)

template<class BaseTableT, class IndexTableT>
class MultiSortedTable_M_Iterator :
	public boost::random_access_iterator_helper<
			MultiSortedTable_M_Iterator<BaseTableT, IndexTableT>,
			typename BaseTableT::value_type,
			ptrdiff_t,
			TERARK_M_const typename BaseTableT::value_type*,
			TERARK_M_const typename BaseTableT::value_type&
		>
{
	typedef MultiSortedTable_M_Iterator<BaseTableT, IndexTableT> my_type;
	friend class MultiSortedTable<BaseTableT>;
	typedef MultiSortedTable<BaseTableT> table_t;
public:
	typedef typename IndexTableT::TERARK_M_iterator indexed_iter_t;
	typedef typename BaseTableT::value_type  value_type;
	typedef TERARK_M_const typename BaseTableT::value_type& reference;
	typedef ptrdiff_t difference_type;

	typedef typename std::vector<value_type>::TERARK_M_iterator         val_iter_t;
	typedef typename std::vector<value_type>::TERARK_M_reverse_iterator rev_val_iter_t;

	typedef typename std::vector<value_type>::const_iterator         c_val_iter_t;
	typedef typename std::vector<value_type>::const_reverse_iterator c_rev_val_iter_t;

protected:
	indexed_iter_t	m_iter;
	TERARK_M_const  table_t* m_owner;
	uint32_t	m_val_index; //!< index num of table_t::m_values

protected:
	MultiSortedTable_M_Iterator(TERARK_M_const table_t* tab, indexed_iter_t iter)
	{
		m_owner = tab;
		m_iter = iter;
		m_val_index = *iter; // if iter == m_owner->m_index.end(), *iter == m_owner->m_values.size()
	}

	MultiSortedTable_M_Iterator(TERARK_M_const table_t* tab, indexed_iter_t iter, ptrdiff_t val_index)
	{
		assert(val_index >= iter[0]);
		assert(val_index <  iter[1]);
		m_owner = tab;
		m_iter = iter;
		m_val_index = val_index;
	}

	bool is_end() const	{ return m_owner->m_values.size() == m_val_index; }

	void synchronizeIter()
	{
		m_iter = std::lower_bound(m_owner->m_index.begin(), m_owner->m_index.end(), m_val_index);
		assert(m_val_index <= *m_iter);
		if (*m_iter != m_val_index)
		{
			--m_iter;
		}
	}

public:

#ifdef TERARK_MULTI_SORTED_TABLE_IS_CONST_ITER
	MultiSortedTable_M_Iterator(const MultiSortedTable_iterator<BaseTableT, IndexTableT>& iter)
		: m_owner(iter.m_owner)
		, m_iter(iter.m_iter)
		, m_val_index(iter.m_val_index)
	{
	}
#else
	friend class MultiSortedTable_const_iterator<BaseTableT, IndexTableT>;
#endif

	MultiSortedTable_M_Iterator() { m_owner = 0; }

	val_iter_t begin() TERARK_M_const {	return m_owner->m_values.begin() + *m_iter;	}
	val_iter_t end()   TERARK_M_const { return m_owner->m_values.begin() + m_iter[1]; }

	rev_val_iter_t rbegin() TERARK_M_const { return rev_val_iter_t(end()); }
	rev_val_iter_t rend()   TERARK_M_const { return rev_val_iter_t(begin()); }

	bool equal_next() const
	{
		assert(!is_end());
		indexed_iter_t next(m_iter); ++next;
		assert(*m_iter <= m_val_index && m_val_index < *next);
		return m_val_index + 1 < *next; // !! must be "m_val_index + 1"
	}

	std::ptrdiff_t distance(const my_type& larger) const
	{
		return std::ptrdiff_t(larger.m_val_index) - std::ptrdiff_t(this->m_val_index);
	}

	/**
	 @brief return equal_count and goto next larger
	 */
	size_t goto_next_larger()
	{
		assert(!is_end());
		assert(m_val_index == *m_iter);

		uint32_t old_val_index = *m_iter;
		indexed_iter_t next(m_iter); ++next;

		m_val_index = *next;
		m_iter = next;

		assert(old_val_index < m_val_index);
		return size_t(m_val_index - old_val_index);
	}

	size_t equal_count() const
	{
		indexed_iter_t next(m_iter); ++next;
		return size_t(*next - *m_iter);
	}

	my_type& operator++()
	{
		assert(!is_end());

		++m_val_index;
		indexed_iter_t next(m_iter); ++next;
		if (m_val_index == *next)
		{
			m_iter = next;
		}
		return *this;
	}
	my_type& operator--()
	{
		assert(0 != m_val_index);

		if (m_val_index == *m_iter)
		{
			--m_iter;
		}
		--m_val_index;
		return *this;
	}

	my_type& operator+=(difference_type diff)
	{
		m_val_index += diff;
		synchronizeIter();
		return *this;
	}

	my_type& operator-=(difference_type diff)
	{
		m_val_index -= diff;
		synchronizeIter();
		return *this;
	}

	friend difference_type operator-(const my_type& x, const my_type& y)
	{
		return x.m_val_index - y.m_val_index;
	}
	friend bool operator<(const my_type& x, const my_type& y)
	{
		return x.m_val_index < y.m_val_index;
	}
	friend bool operator==(const my_type& x, const my_type& y)
	{
		assert(x.m_owner == y.m_owner);
		return x.m_val_index == y.m_val_index;
	}

	reference operator*() const { return m_owner->m_values[m_val_index]; }
};

#undef MultiSortedTable_M_Iterator

#undef TERARK_M_reverse_iterator
#undef TERARK_M_iterator
#undef TERARK_M_const
