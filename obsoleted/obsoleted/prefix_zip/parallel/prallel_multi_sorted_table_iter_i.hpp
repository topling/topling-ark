// file: multi_sorted_table_iter_i.hpp
//
// must included in class MultiSortedTable
// must define:
// -# TERARK_M_const
// -# TERARK_M_iterator
// define TERARK_MULTI_SORTED_TABLE_IS_CONST_ITER when generate const_iterator

#define MultiSortedTable_M_Iterator BOOST_JOIN(parallel_multi_sorted_table_, TERARK_M_iterator)

template<class KeyTable, class ValueT, class TOffset>
class MultiSortedTable_M_Iterator :
	public boost::random_access_iterator_helper<
			MultiSortedTable_M_Iterator<KeyTable, ValueT, TOffset>,
			typename KeyTable::value_type,
			ptrdiff_t,
			TERARK_M_const typename KeyTable::value_type*,
			TERARK_M_const typename KeyTable::value_type&
		>
{
	typedef MultiSortedTable_M_Iterator<KeyTable, ValueT, TOffset> my_type;
	friend class MultiSortedTable<KeyTable>;
	typedef MultiSortedTable<KeyTable> table_t;
public:
	typedef typename KeyTable::TERARK_M_iterator key_tab_iter_t;
	typedef typename KeyTable::value_type  value_type;
	typedef TERARK_M_const typename KeyTable::value_type& reference;
	typedef ptrdiff_t difference_type;

	typedef typename std::vector<value_type>::TERARK_M_iterator         val_iter_t;
	typedef typename std::vector<value_type>::TERARK_M_reverse_iterator rev_val_iter_t;

	typedef typename std::vector<value_type>::const_iterator         c_val_iter_t;
	typedef typename std::vector<value_type>::const_reverse_iterator c_rev_val_iter_t;

protected:
	std::vector<TOffset>::const_iterator m_iter_off;
	key_tab_iter_t	m_iter_key;
	TERARK_M_const  table_t* m_owner;
	size_t			m_val_index; //!< index num of table_t::m_values

protected:
	MultiSortedTable_M_Iterator(TERARK_M_const table_t* tab, key_tab_iter_t iter)
	{
		m_owner = tab;
		m_iter_key = iter;
		m_iter_off = tab->m_ptrvec.begin() + (iter - tab->m_keyset.begin());
		m_val_index = *m_iter_off;
	}

	MultiSortedTable_M_Iterator(TERARK_M_const table_t* tab, key_tab_iter_t iter, ptrdiff_t val_index)
	{
		assert(val_index >= iter[0]);
		assert(val_index <  iter[1]);
		m_owner = tab;
		m_iter_key = iter;
		m_iter_off = tab->m_ptrvec.begin() + (iter - tab->m_keyset.begin());
		m_val_index = val_index;
	}

	bool is_end() const	{ return m_owner->m_values.size() == m_val_index; }

	void synchronizeIter()
	{
		m_iter_key = std::lower_bound(m_owner->m_ptrvec.begin(), m_owner->m_ptrvec.end(), m_val_index);
		m_iter_off = m_owner->m_ptrvec.begin() + (m_iter_key - m_owner->m_keytab.begin());

		assert(m_val_index <= *m_iter_off);
		if (*m_iter_off != m_val_index)
		{
			--m_iter_key;
			--m_iter_off;
		}
	}

public:

#ifdef TERARK_MULTI_SORTED_TABLE_IS_CONST_ITER
	MultiSortedTable_M_Iterator(const MultiSortedTable_iterator<KeyTable, ValueT, TOffset>& iter)
		: m_owner(iter.m_owner)
		, m_iter_key(iter.m_iter_key)
		, m_iter_off(iter.m_iter_off)
		, m_val_index(iter.m_val_index)
	{
	}
#else
	friend class MultiSortedTable_const_iterator<KeyTable, ValueT, TOffset>;
#endif

	MultiSortedTable_M_Iterator() { m_owner = 0; }

	val_iter_t begin() TERARK_M_const {	return m_owner->m_values.begin() + *m_iter_off; }
	val_iter_t end()   TERARK_M_const { return m_owner->m_values.begin() + *(m_iter_off+1); }

	rev_val_iter_t rbegin() TERARK_M_const { return rev_val_iter_t(end()); }
	rev_val_iter_t rend()   TERARK_M_const { return rev_val_iter_t(begin()); }

	bool equal_next() const
	{
		assert(!is_end());
		TOffset next_off = *(m_iter_off+1);
		assert(*m_iter_off <= m_val_index && m_val_index < next_off);
		return m_val_index + 1 < next_off; // !! must be "m_val_index + 1"
	}

	difference_type distance(const my_type& larger) const
	{
		return difference_type(larger.m_val_index) - difference_type(this->m_val_index);
	}

	/**
	 @brief return equal_count and goto next larger
	 */
	size_t goto_next_larger()
	{
		assert(!is_end());

		TOffset low_val_index = *m_iter_off;
		assert(m_val_index == low_val_index);

		++m_iter_key;
		++m_iter_off;
		m_val_index = *m_iter_off;

		assert(low_val_index < m_val_index);
		return size_t(m_val_index - low_val_index);
	}

	size_t equal_count() const
	{
		return size_t(m_iter_off[1] - m_iter_off[0]);
	}

	my_type& operator++()
	{
		assert(!is_end());

		++m_val_index;
		if (m_val_index == *m_iter_off)
		{
			++m_iter_key;
			++m_iter_off;
		}
		return *this;
	}
	my_type& operator--()
	{
		assert(0 != m_val_index);

		if (m_val_index == *m_iter_off)
		{
			--m_iter_key;
			--m_iter_off;
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
