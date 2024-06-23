/* vim: set tabstop=4 : */
#ifndef __terark_biway_table_h__
#define __terark_biway_table_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

#include "zstring_table.hpp"
#include "multi_sorted_table.hpp"

namespace terark { namespace prefix_zip {

template<class IndexedTable, class MultipleTable>
class BiWayTable;

template<class IndexedTable, class MultipleTable>
class BiWayTable_ConstIterator;

template<class TableT,
		 class IndexedIter,
		 class MultipleIter,
		 class BaseIteratorDef,
		 class SelfReflected
>
class BiWayTable_Iterator_Base : public BaseIteratorDef
{
protected:
	typedef TableT table_t;
public:
	typedef typename table_t::key_type key_type;
	typedef typename BaseIteratorDef::value_type value_type;
	typedef typename BaseIteratorDef::reference  reference;

protected:
	table_t*	  m_owner;
	bool		  m_is_in_indexed; //!< if end, m_is_in_indexed is true
	IndexedIter   m_indexed_iter;
	MultipleIter  m_multiple_iter;

protected:
	BiWayTable_Iterator_Base(table_t* tab, IndexedIter u, MultipleIter n)
	{
		m_owner = tab;
		m_indexed_iter = u;
		m_multiple_iter = n;
		synchronizeFlag();
	}

	void synchronizeFlag()
	{
		if (m_multiple_iter == m_owner->m_multiple.end())
		{
			m_is_in_indexed = true;
		}
		else if (m_indexed_iter == m_owner->m_indexed.end())
		{
			// if m_multiple_iter is end too, m_is_in_indexed should be true
			// because m_is_in_indexed && m_indexed_iter == end means *this == end

			m_is_in_indexed = false;
		}
		else
		{
			m_is_in_indexed = m_owner->m_indexed.comp()(
				m_owner->m_indexed.raw_key(m_indexed_iter),
				m_owner->m_multiple.raw_key(m_multiple_iter));
		}
	}

	bool is_end() const
	{
		return m_is_in_indexed && m_indexed_iter == m_owner->m_indexed.end();
	}

	template<class ContainerPtr, class IndexedIter2, class MultipleIter2>
	BiWayTable_Iterator_Base(ContainerPtr owner,
							const IndexedIter2& indexed_iter,
							const MultipleIter2& multiple_iter,
							bool  is_in_indexed
							)
		: m_owner(owner)
		, m_indexed_iter(indexed_iter)
		, m_multiple_iter(multiple_iter)
		, m_is_in_indexed(is_in_indexed)
	{
	}

	BiWayTable_Iterator_Base() { m_owner = 0; }

public:
	bool equal_next() const
	{
		assert(!is_end());

		if (m_is_in_indexed)
			return m_owner->m_indexed.equal_next(m_indexed_iter);
		else
			return m_owner->m_multiple.equal_next(m_multiple_iter);
	}

	reference operator*() const
	{
		assert(!is_end());

		if (m_is_in_indexed)
			return *m_indexed_iter;
		else
			return *m_multiple_iter;
	}

	SelfReflected& operator++()
	{
		assert(!is_end());

		if (m_is_in_indexed)
			++m_indexed_iter;
		else
			++m_multiple_iter;

		synchronizeFlag();

		return static_cast<SelfReflected&>(*this);
	}

	ptrdiff_t distance(const SelfReflected& larger)
	{
		ptrdiff_t dist = 0;
		BiWayTable_Iterator_Base iter(*this);
		while (iter != larger)
		{
			dist += iter.goto_next_larger();
		}
		return dist;
	}

	/**
	 @brief return equal_count and goto next larger
	 */
	size_t goto_next_larger()
	{
		assert(!is_end());

		size_t equal_count;
		if (m_is_in_indexed)
			equal_count = m_owner->m_indexed.goto_next_larger(m_indexed_iter);
		else
			equal_count = m_owner->m_multiple.goto_next_larger(m_multiple_iter);
		synchronizeFlag();
		return equal_count;
	}

	size_t equal_count() const
	{
		assert(!is_end());

		if (m_is_in_indexed)
			return m_owner->m_indexed.equal_count(m_indexed_iter);
		else
			return m_owner->m_multiple.equal_count(m_multiple_iter);
	}

	friend bool operator==(const SelfReflected& x, const SelfReflected& y)
	{
		assert(x.m_owner == y.m_owner);

		if (x.m_is_in_indexed && y.m_is_in_indexed)
			return x.m_indexed_iter == y.m_indexed_iter;
		else if (!x.m_is_in_indexed && !y.m_is_in_indexed)
			return x.m_multiple_iter == y.m_multiple_iter;
		else
			return false;
	}
};
#define TERARK_BI_WAY_TABLE_ITERATOR_BASE(TableClass, IterClass, ConstVal, IterType) \
	BiWayTable_Iterator_Base<						\
		TableClass<IndexedTable, MultipleTable>,	\
		typename IndexedTable::IterType,			\
		typename MultipleTable::IterType,			\
		boost::forward_iterator_helper<				\
			IterClass<IndexedTable, MultipleTable>,	\
			ConstVal IndexedTable::value_type,		\
			ptrdiff_t,								\
			ConstVal IndexedTable::value_type*,		\
			ConstVal IndexedTable::value_type&		\
		>,											\
		IterClass<IndexedTable, MultipleTable> >
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

template<class IndexedTable, class MultipleTable>
class BiWayTable_Iterator :
	public TERARK_BI_WAY_TABLE_ITERATOR_BASE(BiWayTable, BiWayTable_Iterator, typename, iterator)
{
	friend class BiWayTable<IndexedTable, MultipleTable>;
	friend class BiWayTable_ConstIterator<IndexedTable, MultipleTable>;

	typedef BiWayTable<IndexedTable, MultipleTable> table_t;
	typedef TERARK_BI_WAY_TABLE_ITERATOR_BASE(BiWayTable, BiWayTable_Iterator, typename, iterator) super;

public:
	typedef typename MultipleTable::iterator  multiple_iter_t;
	typedef typename  IndexedTable::iterator   indexed_iter_t;

public:
	BiWayTable_Iterator(table_t* tab, indexed_iter_t u, multiple_iter_t n)
		: super(tab, u, n)
	{}
	BiWayTable_Iterator() {}
};

template<class IndexedTable, class MultipleTable>
class BiWayTable_ConstIterator :
	public  TERARK_BI_WAY_TABLE_ITERATOR_BASE(const BiWayTable, BiWayTable_ConstIterator, const typename, const_iterator)
{
	typedef TERARK_BI_WAY_TABLE_ITERATOR_BASE(const BiWayTable, BiWayTable_ConstIterator, const typename, const_iterator) super;

	typedef BiWayTable<IndexedTable, MultipleTable> table_t;
	friend class BiWayTable<IndexedTable, MultipleTable>;

public:
	typedef typename MultipleTable::const_iterator multiple_iter_t;
	typedef typename  IndexedTable::const_iterator  indexed_iter_t;

public:
	BiWayTable_ConstIterator() {}
	BiWayTable_ConstIterator(const table_t* tab, indexed_iter_t u, multiple_iter_t n)
		: super(tab, u, n)
	{}
	BiWayTable_ConstIterator(BiWayTable_Iterator<IndexedTable, MultipleTable> iter)
		: super(iter.m_owner, iter.m_indexed_iter, iter.m_multiple_iter, iter.m_is_in_indexed)
	{ }
};

/**
 @brief BiWayTable 可以有效包含重复元素的 ZStringTable

  但是比 SortedTable<key_type, value_type, CompareT> 的存储效率更高，
  按 key 查询遍历 value 集合时速度也更快，主要原因是字符串解压速度会比较快
 */
template<class IndexedTable, class MultipleTable>
class BiWayTable
{
	friend class TERARK_BI_WAY_TABLE_ITERATOR_BASE(BiWayTable, BiWayTable_Iterator, typename, iterator);
	friend class TERARK_BI_WAY_TABLE_ITERATOR_BASE(const BiWayTable, BiWayTable_ConstIterator, const typename, const_iterator);

	friend class BiWayTable_Iterator     <IndexedTable, MultipleTable>;
	friend class BiWayTable_ConstIterator<IndexedTable, MultipleTable>;

public:
	/**
	 将 key-{value} 转存到 m_indexed 时的 value 数目最佳值

	 - key 相同的 value 数据超过这个数目时，就将 key-{value} 集合放入 m_indexed 中，有助于
	   提高存储和查询效率，因为在 m_indexed 中，需要多存一个 uint32_t 来表示value_type 的下标，
	   而如果放在 m_multiple 中，当存储多个 value_type 时，需要为每个重复 value_type 多存一个
	   uint32_t 的 string_pool.offset。
	 - 在这里计算出一个最佳值，真正的最佳值可能要比按 value_type 的最小存储需求的最佳值大一点，因为
	   如果分开在了两个 ZStringTable 中，字符串的公共前缀少了，从而导致字符串的压缩率下降，
	   因此应该稍微大一点（所以就在这个基础上增加了 2）。
	 - 计算 n 的方程式：
	   sizeof(value_type)*n + sizeof(uint32_t) == (sizeof(value_type) + sizeof(uint32_t)) * n
	   所以：n = 1
	 */
	BOOST_STATIC_CONSTANT(uint_t, MAX_MULTI_DUP_COUNT = 3);

	struct dump_header
	{
		uint32_t version;
		typename IndexedTable::dump_header  indexed;
		typename MultipleTable::dump_header multiple;

		template<class SrcTable>
		explicit dump_header(const SrcTable& src)
			: indexed(src), multiple(src)
		{
			version = 0;
		}
		explicit dump_header(const BiWayTable& src)
			: indexed(src.m_indexed), multiple(src.m_multiple)
		{
			version = 0;
		}

		dump_header()
		{
			version = 0;
		}

		bool less_than(const dump_header& loaded) const
		{
			return multiple.less_than(loaded.multiple) || indexed.less_than(loaded.indexed);
		}

		void check()
		{
			indexed.check();
			multiple.check();
		}

		template<class Input> void load(Input& input, uint32_t version)
		{
			this->version = version;
			input >> indexed >> multiple;
		}
		template<class Output> void save(Output& output, uint32_t version) const
		{
			output << indexed << multiple;
		}

		DATA_IO_REG_LOAD_SAVE_V(dump_header, 1)
	};
	friend struct dump_header;

	/**
	 @brief 用较小的尺寸表示 key 指针，只能指向 key，不能指向同一 key 的多个 value 中的具体某个

	  handle 的尺寸比 iterator 小得多（iterator 在 32 位系统上占 16 个字节，handle 总是占 4 个字节）
	  用于需要存储大量指向 key 的指针时，如统计所有 key 的频率
	 */
	BOOST_STRONG_TYPEDEF(uint32_t, handle_t);

	typedef typename IndexedTable::compare_t compare_t;

	/**
	 @brief 从 key 到一个 uint32_t 的映射表

	 这个 uint32_t 存储的是这个 key 的第一个 value 在 m_val_array 中的下标，
	 这个 key 对应的 value 数目由下一个下标和当前下标的差计算出来，
	 因为 m_indexed.end() 实际上指向一个末尾的哑元，所以使用 *m_indexed.end() 存储
	 m_val_array 的尺寸，这样对 m_indexed 的处理就很方便，不必再单独为末尾的元素做不同的处理。
	 （ ZStringTable 本身末尾就有一个哑元，这里刚好利用了它）。
	 */
	typedef IndexedTable indexed_table_t;

	/**
	 @brief 从 key 到 value_type 的倒排表

	 在这个倒排表中，一个 key 最多可以对应的 value 是 MAX_MULTI_DUP_COUNT，
	 超过这个数目，key-{value} 就会转移到 m_multiple 和 m_val_array 中。因为 value
	 数目较小时这样存储更有效率。
	 */
	typedef MultipleTable multiple_table_t;

	typedef typename MultipleTable::raw_key_t  raw_key_t;
	typedef typename MultipleTable::key_type   key_type;
	typedef typename MultipleTable::value_type value_type;
	typedef typename MultipleTable::size_type  size_type;

	typedef BiWayTable_Iterator<IndexedTable, MultipleTable>		    iterator;
	typedef BiWayTable_ConstIterator<IndexedTable, MultipleTable> const_iterator;

	typedef typename iterator::difference_type difference_type;

	typedef std::pair<iterator, iterator> range_t;
	typedef std::pair<const_iterator, const_iterator> const_range_t;

private:
	indexed_table_t	 m_indexed;
	multiple_table_t m_multiple;
	uint32_t m_lastDupCount;

public:
	explicit BiWayTable(uint32_t maxPoolSize, compare_t comp = compare_t())
		: m_indexed(maxPoolSize, comp), m_multiple(maxPoolSize, comp)
	{
		m_lastDupCount = 0;
		m_multiple.set_unzip_prefer(uzp_small_dup);
	}

	void set_unzip_prefer(unzip_prefer uzp)
	{
		// do nothing...
		// m_multiple.set_unzip_prefer(uzp);
	}

	compare_t comp() const { return m_indexed.comp(); }

//////////////////////////////////////////////////////////////////////////
	void swap(BiWayTable& that)
	{
		m_indexed.swap(that.m_indexed);
		m_multiple.swap(that.m_multiple);
		std::swap(m_lastDupCount, that.m_lastDupCount);
	}
	friend void swap(BiWayTable& x, BiWayTable& y) { x.swap(y); }

	void clear()
	{
		m_indexed.clear();
		m_multiple.clear();
		m_lastDupCount = 0;
	}

	void reserve(size_type valCount, size_type keyCount = 0)
	{
		if (0 == keyCount) keyCount = valCount;
		m_multiple.reserve(valCount);
		m_indexed.reserve(keyCount);
	}

	/**
	 @{
	 @brief 为 iter 指向的 key 关联的 values 排序
	 @return 返回排序的 value_count
	 @note iter 必须指向 key-values 中的第一个 value
	 */
	template<class Compvalue_type>
	size_t sort_values(iterator iter, Compvalue_type compVal)
	{
		if (iter.m_is_in_indexed)
			return m_indexed.sort(iter.m_indexed_iter, compVal);
		else
			return m_multiple.sort(iter.m_multiple_iter, compVal);
	}
	template<class Compvalue_type>
	size_t sort_values(handle_t handle, Compvalue_type compVal)
	{
		if (handle.t & 0x80000000)
			return m_indexed.sort(typename indexed_table_t::handle_t(handle.t & ~0x80000000));
		else
			return m_multiple.sort(typename multiple_table_t::handle_t(handle.t), compVal);
	}
	//@}

	//! sort all values associated with their key
	template<class Compvalue_type>
	void sort_values(Compvalue_type compVal)
	{
		m_multiple.sort_values(compVal);
		m_indexed.sort_values(compVal);
	}

	size_type key_count() const	{ return m_indexed.size() + m_multiple.key_count(); }
	size_type val_count() const	{ return m_indexed.size() + m_multiple.size(); }
//	size_type lastDupCount() const { return m_lastDupCount; }

	//! same as key_count
//	size_type distinct_size() const	{ return key_count(); }

	bool empty() const { return m_indexed.empty() && m_multiple.empty(); }
	size_type size() const	{ return val_count(); }

	key_type key(handle_t handle) const
	{
		if (handle.t & 0x80000000)
			return m_indexed.key(indexed_table_t::handle_t(handle.t & ~0x80000000));
		else
			return m_multiple.key(multiple_table_t::handle_t(handle.t));
	}

////////////////////////////////////////////////////////////////////////

	const key_type& lastKey() const
	{
		if (m_lastDupCount > MAX_MULTI_DUP_COUNT)
			return m_indexed.lastKey();
		else
			return m_multiple.lastKey();
	}

#define TERARK_M_const const
#define TERARK_M_iterator const_iterator
#define TERARK_M_reverse_iterator const_reverse_iterator
#include "biway_table_mf.hpp"

#define TERARK_M_const
#define TERARK_M_iterator iterator
#define TERARK_M_reverse_iterator reverse_iterator
#include "biway_table_mf.hpp"

protected:
	template<class TableT>
	static string_appender<>& writeInfo(string_appender<>& oss, const char* title, const TableT& tab)
	{
		oss << title << "[size=" << tab.size()
			<< ", pool[used=" << tab.used_pool_size()
			<< ", total=" << tab.total_pool_size()
			<< ", lastKey=" << tab.lastKey() << "]]";
		return oss;
	}

public:
	/**
	 @{
	 @brief push_back key-value(s)
	 */
	void push_back(const key_type& xkey, const value_type& val);

	template<class IterT>
	void push_back(const key_type& xkey, const IterT& first, const IterT& last)
	{
		if (first == last) return;
		push_back_sequence(xkey, first, last, typename IterT::iterator_category());
	}

	/**
	 @brief make sure std::distance(first, last) == count
	 */
	template<class IterT>
	void push_back(const key_type& xkey, const IterT& first, const IterT& last, uint32_t count)
	{
	//	if (first == last) return; // not needed

		int icomp = empty() ? -1 : EffectiveCompare(comp(), lastKey(), xkey);
		if (icomp > 0)
			raise_error(xkey, BOOST_CURRENT_FUNCTION, __FILE__, __LINE__);

		uint32_t curLastDupCount = (0 == icomp) ? count + m_lastDupCount : count;
		size_t old_val_count = this->val_count();

		if (curLastDupCount > MAX_MULTI_DUP_COUNT)
			m_indexed.push_back(xkey, first, last);
		else
			m_multiple.push_back(xkey, first, last);

		//! push_back may throw exception
		//! update state after push_back success
		m_lastDupCount = curLastDupCount;

		//! count must equal to std::distance(first, last)
		//! because std::distance(first, last) maybe use much time
		//! so assert as below:
		//!
#if defined(_DEBUG) || defined(DEBUG) || !defined(NDEBUG)
		long count2 = std::distance(first, last);
		assert(count == count2);
#endif
		size_t new_val_count = val_count();
		TERARK_RT_assert(old_val_count + count == new_val_count, std::invalid_argument);
	}
	//@}

	bool can_insert(const key_type& xkey) const
	{
		return m_indexed.can_insert(xkey) && m_multiple.can_insert(xkey);
	}

	void fast_insert(const key_type& xkey, const value_type& val)
	{
		typename indexed_table_t::iterator iter1 = m_indexed.find(xkey);
		if (m_indexed.end() != iter1)
		{
			m_indexed.insert(xkey, val);
			return;
		}
		typename multiple_table_t::range_t range = m_multiple.equal_range(xkey);
		if (std::distance(range.first, range.second) < MAX_MULTI_DUP_COUNT)
		{
			m_multiple.insert(xkey, val);
		}
		else
		{
			m_indexed.insert(xkey, range.first, range.second);
			m_indexed.insert(xkey, val);
			m_multiple.erase(range.first, range.second);
		}
	}

	iterator insert(const key_type& xkey, const value_type& val)
	{
		typename indexed_table_t::iterator iter1 = m_indexed.find(xkey);
		if (m_indexed.end() != iter1)
		{
			return iterator(this, m_indexed.insert(xkey, val), m_multiple.upper_bound(xkey));
		}
		typename multiple_table_t::range_t range = m_multiple.equal_range(xkey);
		if (std::distance(range.first, range.second) < MAX_MULTI_DUP_COUNT)
		{
			return iterator(this, m_indexed.upper_bound(xkey), m_multiple.insert(xkey, val));
		}
		else
		{
			m_indexed.insert(xkey, range.first, range.second);
			iter1 = m_indexed.insert(xkey, val);

			m_multiple.erase(range.first, range.second);

			return iterator(this, iter1, range.second);
		}
	}

	template<class IterT>
	void insert(const key_type& xkey, const IterT& first, const IterT& last)
	{
		typename indexed_table_t::iterator iter1 = m_indexed.find(xkey);
		if (m_indexed.end() != iter1)
		{
			m_indexed.insert(xkey, first, last);
			return ;
		}
		typename multiple_table_t::range_t range = m_multiple.equal_range(xkey);
		if (std::distance(range.first, range.second) + std::distance(first, last) <= MAX_MULTI_DUP_COUNT)
		{
			m_multiple.insert(xkey, first, last);
		}
		else
		{
			m_indexed.insert(xkey, range.first, range.second);
			m_indexed.insert(xkey, first, last);
			m_multiple.erase(range.first, range.second);
		}
	}

	template<class TableT, class IterT>
	void insert(const TableT& other, IterT first, IterT last)
	{
		for (IterT i = first; i != last; ++i)
		{
			insert(other.raw_key(i), *i);
		}
	}
	template<class TableT>
	void insert(const TableT& other)
	{
		insert(other, other.begin(), other.end());
	}

	bool is_zipped(const_iterator iter) const
	{
		if (iter.m_is_in_indexed)
			return m_indexed.is_zipped(iter.m_indexed_iter);
		else
			return m_multiple.is_zipped(iter.m_multiple_iter);
	}

	size_t unzip_key_size() const { return m_indexed.unzip_key_size() + m_multiple.unzip_key_size(); }

	size_t  used_pool_size() const { return m_indexed. used_pool_size() + m_multiple. used_pool_size(); }
	size_t total_pool_size() const { return m_indexed.total_pool_size() + m_multiple.total_pool_size(); }
	size_t available_pool_size() const { return m_indexed.available_pool_size() + m_multiple.available_pool_size(); }

	size_t  used_mem_size() const { return m_multiple. used_mem_size() + m_indexed. used_mem_size(); }
	size_t total_mem_size() const { return m_multiple.total_mem_size() + m_indexed.total_mem_size(); }

	/**
	 @brief 释放浪费的空间

	  如果可用的空间大于 5\%，就新申请一块内存存放字符串池，并将原先的内存释放\n
	  一般用于冻结一个 BiWayTable 时，调用该函数之后，再加入新元素很可能失败
	 */
	void trim_extra()
	{
		m_indexed.trim_extra();
		m_multiple.trim_extra();
	}

//////////////////////////////////////////////////////////////////////////
private:
	void raise_error(const key_type& xkey, const char* func, const char* file, int line)
	{
		string_appender<> oss;
		oss << file << ":" << line << ", key='" << xkey;
		if (empty())
			oss << ", is empty";
		else
			oss << "', lastKey='" << lastKey();
		oss	<< "', key must not less than lastKey.\n"
			<< "in function: " << func << "\n"
			<< "other member[lastDupCount=" << m_lastDupCount << ",\n";
		writeInfo(oss, "   total___", *this) << "\n";
		writeInfo(oss, "   indexed_", m_indexed) << "\n";
		writeInfo(oss, "   multiple", m_multiple) << "\n";
		oss << "   val_array.size=" << m_indexed.size() << ",\n"
			<< "]";
		fprintf(stderr, "%s\n", oss.str().c_str());
		throw std::logic_error(oss.str().c_str());
	}
	template<class IterT>
	void push_back_sequence(const key_type& xkey, const IterT& first, const IterT& last, std::forward_iterator_tag)
	{
		int icomp = empty() ? -1 : EffectiveCompare(comp(), lastKey(), xkey);
		if (icomp > 0)
			raise_error(xkey, BOOST_CURRENT_FUNCTION, __FILE__, __LINE__);

		uint32_t count = (0 == icomp) ? m_lastDupCount : 0;

		for (IterT iter = first; iter != last; ++iter)
		{
			count++;
			if (count > MAX_MULTI_DUP_COUNT)
				// 只探测到这么大，可能 [first, last) 有好几万，探测再多也无必要
				break;
		}
		if (count > MAX_MULTI_DUP_COUNT) {
			size_t old_size = m_indexed.val_count();
			m_indexed.push_back(xkey, first, last);
			count = m_indexed.val_count() - old_size; // actual pushed val count
		} else {
			size_t old_size = m_multiple.size();
			m_multiple.push_back(xkey, first, last);
			count = m_multiple.size() - old_size; // actual pushed val count
		}
		if (0 == icomp)
			m_lastDupCount += count;
		else
			m_lastDupCount = count;
	}

	template<class IterT>
	void push_back_sequence(const key_type& xkey, const IterT& first, const IterT& last, std::random_access_iterator_tag)
	{
		assert(first < last);
		push_back(xkey, first, last, last - first);
	}

public:
	template<class ResultContainer, class CompatibleCompare>
	void query_result(const key_type& xkey, ResultContainer& result, CompatibleCompare comp) const
	{
		copy_range(equal_range(xkey, comp), result);
	}

// 	template<class CompatibleCompare>
// 	std::vector<value_type> query_result(const key_type& xkey, const CompatibleCompare comp) const
// 	{
// 		std::vector<value_type> result;
// 		copy_range(equal_range(xkey, comp), result);
// 		return result;
// 	}

	template<class ResultContainer>
	static void copy_range(const_range_t range, ResultContainer& set)
	{
		while (range.first != range.second)
		{
			set.push_back(*range.first);
			++range.first;
		}
	}

	//! more efficient than CompatibleCompare version

	template<class ResultContainer>
	void query_result(const key_type& xkey, ResultContainer& result) const
	{
		typename indexed_table_t::const_range_t range = m_indexed.equal_range(xkey);
		if (range.first != range.second)
			std::copy(range.first, range.second, std::back_inserter(result));
		else
			m_multiple.query_result(xkey, result);
	}

	std::vector<value_type> query_result(const key_type& xkey) const
	{
		std::vector<value_type> result;
		query_result(xkey, result);
		return result;
	}

	void init(uint32_t maxPoolSize)
	{
		m_indexed.init(maxPoolSize);
		m_multiple.init(maxPoolSize);
	}

	static void check_key(const key_type& xkey) { indexed_table_t::check_key(xkey); }

	template<class DataInput>
	void dump_load_segment(DataInput& input, dump_header& loading, const dump_header& loaded)
	{
		m_multiple.dump_load_segment(input, loading.multiple, loaded.multiple);
		m_indexed.dump_load_segment(input, loading.indexed, loaded.indexed);
	}
	void prepair_dump_load_merged(const dump_header& loaded)
	{
		m_multiple.prepair_dump_load_merged(loaded.multiple);
		m_indexed.prepair_dump_load_merged(loaded.indexed);
	}
	template<class DataInput>
	void complete_dump_load_merged(DataInput& input, dump_header& loading, const dump_header& loaded)
	{
		m_multiple.complete_dump_load_merged(input, loading.multiple, loaded.multiple);
		m_indexed.complete_dump_load_merged(input, loading.indexed, loaded.indexed);
		trim_extra();

		if (comp()(m_multiple.lastKey(), m_indexed.lastKey())) {
			typename indexed_table_t::iterator iback = m_indexed.end();
			m_lastDupCount = m_indexed.equal_count(--iback);
		} else
			m_lastDupCount = m_multiple.end() - m_multiple.first_equal(m_multiple.end()-1);
	}

	template<class DataOutput>
	void dump_save_segment(DataOutput& output, dump_header& header) const
	{
		m_multiple.dump_save_segment(output, header.multiple);
		m_indexed.dump_save_segment(output, header.indexed);
	}

	template<class DataInput > void dump_load(DataInput& input)
	{
		this->clear();

		dump_header header;
		input >> header;
		input >> m_lastDupCount;
		header.check();
	//	printf("count[key=%d, lastDup=%d, val=%d]\n", key_count(), m_lastDupCount, header.valArraySize);

		m_multiple.dump_load(input);
		m_indexed.dump_load(input);
	}
	template<class DataOutput> void dump_save(DataOutput& output) const
	{
		dump_header header(*this);
		output << header;
		output << m_lastDupCount;

		m_multiple.dump_save(output);
		m_indexed.dump_save(output);
	}
	DATA_IO_REG_DUMP_LOAD_SAVE(BiWayTable)

protected:
	template<class DataInput > void load(DataInput& input, unsigned int version)
	{
		dump_header header;
		input >> header;
		input >> m_lastDupCount;
		input >> m_multiple;
		input >> m_indexed;
	}
	template<class DataOutput> void save(DataOutput& output, unsigned int version) const
	{
		dump_header header(*this);
		output << header;
		output << m_lastDupCount;
		output << m_multiple;
		output << m_indexed;
	}
	DATA_IO_REG_LOAD_SAVE_V(BiWayTable, 1)
};

// push_back 元素的顺序必须是字典顺序
template<class IndexedTable, class MultipleTable>
void BiWayTable<IndexedTable, MultipleTable>::push_back(const key_type& xkey, const value_type& val)
{
	// const int offset_size = sizeof(uint32_t);
	// int n = m_lastDupCount;
	// int max = MAX_MULTI_DUP_COUNT
	// n 个元素存在 m_multiple 中的尺寸： n*offset_size + n*sizeof(value_type)
	//    每个元素的 key zstring_pool 的 offset 都相同，连续地存放在 vector 中
	//
	// n 个元素存在 m_indexed 中的尺寸： max*offset_size + n*sizeof(value_type)
	//    一个 offset 是 zstring_pool 的 offset，另一个是 m_val_array 的下标
	//
	// 把一个 ZStringTable 拆成两个，导致了压缩率的降低，因此如果 max 选得小了（最小是 2）
	// 存储的 offset 是少了，但是压缩率降低了，string_pool 占用的空间却变大了
	// 因此 max 应该选得大一点 @see MAX_MULTI_DUP_COUNT

	if (m_lastDupCount >= MAX_MULTI_DUP_COUNT)
	{
		if (MAX_MULTI_DUP_COUNT == m_lastDupCount)
		{
			// lastKey is m_multiple.lastKey()

			if (!m_multiple.comp()(m_multiple.lastKey(), xkey)) {
				m_indexed.push_back(xkey, m_multiple.end()-MAX_MULTI_DUP_COUNT, m_multiple.end());
				m_indexed.push_back(xkey, val);
				m_multiple.pop_back_n(MAX_MULTI_DUP_COUNT);
			} else {
				m_multiple.push_back(xkey, val);
				m_lastDupCount = 0;
			}
		}
		else // m_lastDupCount > MAX_MULTI_DUP_COUNT, lastKey is m_indexed.lastKey()
		{
			if (!m_indexed.comp()(m_indexed.lastKey(), xkey)) // ! lk < key --> key <= lk, so lk == key
				m_indexed.push_back(xkey, val);
			else {
				m_multiple.push_back(xkey, val);
				m_lastDupCount = 0;
			}
		}
	}
	else // m_lastDupCount < MAX_MULTI_DUP_COUNT
	{
		m_multiple.push_back(xkey, val);
		if (m_multiple.size() > 1 && !m_multiple.equal_next(m_multiple.end()-2))
			// 'key' is larger than 'lastKey()'
			m_lastDupCount = 0;
	}
	m_lastDupCount++;
}

//////////////////////////////////////////////////////////////////////////

template<class ValueT, class CompareT, uint_t FlagLength>
class BiWayZippedStringTable : public BiWayTable<
		MultiSortedTable<ZStringTable<ValueT, CompareT, FlagLength, false> >,
		ZStringTable<ValueT, CompareT, FlagLength, false> >
{
	typedef BiWayTable<
		MultiSortedTable<ZStringTable<ValueT, CompareT, FlagLength, false> >,
		ZStringTable<ValueT, CompareT, FlagLength, false> > super;
public:
	BiWayZippedStringTable(uint32_t maxPoolSize, CompareT comp = CompareT())
		: super(maxPoolSize, comp)
	{}
};


} } // namespace terark::prefix_zip


#endif // __terark_biway_table_h__

