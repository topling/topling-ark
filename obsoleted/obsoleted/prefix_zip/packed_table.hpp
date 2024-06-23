/* vim: set tabstop=4 : */
#ifndef __packed_table_h__
#define __packed_table_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

#include <assert.h>

#include "../DataBuffer.hpp"
#include <terark/io/DataIO.hpp>
#include <terark/io/InputStream.h>

#include "../string_compare.hpp"

#include "string_pool.hpp"
#include "key_pool.hpp"
#include "unzip_prefer.hpp"
#include "dump_header.hpp"

#include <boost/tuple/tuple.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/ordered_index.hpp>

namespace terark { namespace prefix_zip {

template<class KeyT, class ValueT, class CompareT, bool IsUniqueKey>
class PackedTableImpl
{
public:
	typedef typename KeyValueNode<KeyT, ValueT, CompareT>::type node_t;
	typedef boost::multi_index::const_mem_fun<node_t, const KeyT&, &node_t::key> key_extractor_t;

	typedef	boost::multi_index::multi_index_container<
		node_t, // (key, value)
		boost::multi_index::indexed_by<
			typename boost::mpl::if_c<IsUniqueKey,
				boost::multi_index::ordered_unique    <key_extractor_t, CompareT>,
				boost::multi_index::ordered_non_unique<key_extractor_t, CompareT>
			>::type // if_c
		> // indexed_by
	>// multi_index_container
	index_t;
	typedef KeyPool<KeyT> key_pool_t;

	static key_extractor_t make_key_extractor(key_pool_t* ppool) { return key_extractor_t(); }
};

template<class ValueT, class CompareT, bool IsUniqueKey>
class PackedTableImpl<std::string, ValueT, CompareT, IsUniqueKey>
{
public:
	typedef typename IndexNode<ValueT>::type node_t;
	typedef StringPool::KeyExtractor<ValueT> key_extractor_t;

	typedef	boost::multi_index::multi_index_container<
		node_t, // (offset, value)
		boost::multi_index::indexed_by<
			typename boost::mpl::if_c<IsUniqueKey,
				boost::multi_index::ordered_unique    <key_extractor_t, CompareT>,
				boost::multi_index::ordered_non_unique<key_extractor_t, CompareT>
			>::type // if_c
		> // indexed_by
	>// multi_index_container
	index_t;
	typedef StringPool key_pool_t;

	static key_extractor_t make_key_extractor(StringPool* ppool) { return key_extractor_t(ppool); }
};

/// 提前声明
template<class KeyT,
		 class ValueT,
		 class CompareT =
			typename boost::mpl::if_c<
				boost::is_same<KeyT, std::string>::value,
				StringCompareSZ,
				std::less<KeyT>
			>::type,
		 bool IsUniqueKey=true
	>
class PackedTable;

/// 提前声明
template<class KeyT, class ValueT, class CompareT, bool IsUniqueKey>
class PackedTable_ConstIterator;

/**
 @brief 迭代器定义
 */
template<class KeyT, class ValueT, class CompareT, bool IsUniqueKey>
class PackedTable_Iterator :
	public boost::bidirectional_iterator_helper<
				PackedTable_Iterator<KeyT, ValueT, CompareT, IsUniqueKey>,
				ValueT,
				std::ptrdiff_t,
				ValueT*,
				ValueT&>
{
	friend class PackedTable<KeyT, ValueT, CompareT, IsUniqueKey>;
	friend class PackedTable_ConstIterator<KeyT, ValueT, CompareT, IsUniqueKey>;
	typedef PackedTable<KeyT, ValueT, CompareT, IsUniqueKey> table_t;
	typedef typename table_t::node_t  node_t;
	typedef typename table_t::index_t index_t;

public:
	typedef typename index_t::iterator index_iter_t;

private:
	typename index_t::iterator m_iter;

public:

	PackedTable_Iterator(typename index_t::iterator iter)
		: m_iter(iter) {}
	PackedTable_Iterator() {}

	ValueT& operator*() const { return const_cast<ValueT&>((*m_iter).val()); }

	PackedTable_Iterator& operator++() { ++m_iter; return *this; }
	PackedTable_Iterator& operator--() { --m_iter; return *this; }

	friend bool operator==(const PackedTable_Iterator& x, const PackedTable_Iterator& y)
	{
		return x.m_iter == y.m_iter;
	}
};

/**
 @brief 常量迭代器定义
 */
template<class KeyT, class ValueT, class CompareT, bool IsUniqueKey>
class PackedTable_ConstIterator :
	public boost::bidirectional_iterator_helper<
				PackedTable_ConstIterator<KeyT, ValueT, CompareT, IsUniqueKey>,
				ValueT,
				std::ptrdiff_t,
				ValueT*,
				ValueT&>
{
	friend class PackedTable<KeyT, ValueT, CompareT, IsUniqueKey>;

	typedef PackedTable<KeyT, ValueT, CompareT, IsUniqueKey> table_t;
	typedef typename table_t::node_t   node_t;
	typedef typename table_t::index_t  index_t;
	typename index_t::const_iterator m_iter;

public:
	typedef typename index_t::const_iterator index_iter_t;

	PackedTable_ConstIterator(typename index_t::const_iterator iter)
		: m_iter(iter) {}
	PackedTable_ConstIterator() {}
	PackedTable_ConstIterator(PackedTable_Iterator<KeyT, ValueT, CompareT, IsUniqueKey> iter)
		: m_iter(iter.m_iter) { }

	/**
	 @brief now, only used for MultiPackedTable
	 */
	const node_t& get_node() const { return *m_iter; }

	const ValueT& operator*() const { return (*m_iter).val(); }

	PackedTable_ConstIterator& operator++() { ++m_iter; return *this; }
	PackedTable_ConstIterator& operator--() { --m_iter; return *this; }

	friend bool operator==(const PackedTable_ConstIterator& x, const PackedTable_ConstIterator& y)
	{
		return x.m_iter == y.m_iter;
	}
};

/**
 @brief 紧缩字符串的映射表

 @param ValueT 要映射的用户对象
 @param CompareT 用户提供的比较对象【提前声明中默认为 StringCompareSZ】

 - 内部使用 boost::multi_index 实现
 - 比起直接使用 std::string 作为键类型，每个元素要节省约 20 个字节，甚至更多\n
   因为 std::string 对象本身和 string 中字符所占用的堆空间都比使用 StringPool 和偏移表示的要大\n
   但是 boost::multi_index 索引节点仍要占据 16 个字节的额外空间\n
   【multi_index 采用红黑树实现，每个节点有3个指针和一个bool】
 */
template<class KeyT, class ValueT, class CompareT, bool IsUniqueKey>
class PackedTable
{
	DECLARE_NONE_COPYABLE_CLASS(PackedTable)

	typedef PackedTableImpl<KeyT, ValueT, CompareT, IsUniqueKey> impl_t;

public:
	BOOST_STATIC_CONSTANT(uint_t, MAX_STRING = 65535);

	typedef Table_dump_header     dump_header;
	typedef typename impl_t::node_t		node_t;
	typedef typename node_t::CompID		CompID;
	typedef typename impl_t::index_t	index_t;
	typedef typename impl_t::key_pool_t	key_pool_t;
	typedef typename key_pool_t::raw_key_t raw_key_t;

	typedef typename index_t::size_type	size_type;
	typedef CompareT					compare_t;
	typedef KeyT						key_type;
	typedef ValueT						value_type;
	typedef ValueT&						reference;

	typedef PackedTable_Iterator<KeyT, ValueT, CompareT, IsUniqueKey>			 iterator;
	typedef PackedTable_ConstIterator<KeyT, ValueT, CompareT, IsUniqueKey> const_iterator;

	typedef boost::reverse_iterator<iterator>		reverse_iterator;
	typedef boost::reverse_iterator<const_iterator> const_reverse_iterator;

	typedef std::pair<const_iterator, const_iterator> const_range_t;
	typedef std::pair<iterator, iterator> range_t;
	typedef std::pair<iterator, bool> iter_bool_t;

	typedef boost::integral_constant<bool, IsUniqueKey> is_unique_key_t;
	typedef typename boost::mpl::if_c<IsUniqueKey, iter_bool_t, iterator>::type insert_return_t;

	explicit PackedTable(uint32_t maxKeyPool = TERARK_IF_DEBUG(8*1024, 2*1024*1024),
						 CompareT comp = CompareT())
		: m_index(boost::make_tuple(
			boost::make_tuple(
				impl_t::make_key_extractor(&m_key_pool),
				comp
				)))
		, m_key_pool(maxKeyPool)
	{}

	void clear()
	{
		m_index.clear();
		m_key_pool.reset();
	}

	void erase(iterator first, iterator last)
	{
		m_index.erase(first.m_iter, last.m_iter);
	}

	void erase(iterator iter)
	{
		m_index.erase(iter.m_iter);
	}

	bool can_insert(const KeyT& xkey) const { return m_key_pool.can_insert(xkey); }
	size_type size() const { return m_index.size(); }

	size_type key_count() const { return m_key_pool.key_count(); }
	size_type val_count() const { return m_index.size(); } // same as size

	template<typename CompatibleKey>
	size_type count(const CompatibleKey& x) const
	{
		return count(x, m_index.key_comp());
	}

	template<typename CompatibleKey, typename CompatibleCompare>
	size_type count(const CompatibleKey& x, CompatibleCompare comp) const
	{
		return m_index.node_count(x, comp);
	}

private:
	template<class CompValueT>
	void do_sort_values(iterator first, iterator last, size_type val_count, CompValueT compVal)
	{
		std::vector<ValueT> temp;
		std::vector<iterator> vptr;
		temp.reserve(val_count);
		vptr.reserve(val_count);
		for (iterator j = first; j != last; ++j)
		{
			temp.push_back(*j);
			vptr.push_back(j);
		}
		std::sort(temp.begin(), temp.end(), compVal);
		for (int k = 0; k != vptr.size(); ++k)
		{
			*vptr[k] = temp[k];
		}
	}
public:
	/**
	 @{
	 @brief 为 iter 指向的 key 关联的 values 排序

	 @return 返回排序的 value_count
	 @note iter 必须指向 key-values 中的第一个 value
	 */
	template<class CompValueT>
	size_t sort_values(iterator iter, CompValueT compVal)
	{
		BOOST_STATIC_ASSERT(!is_unique_key_t::value); // unique key has no sort
		assert(iter != end());

		iterator next = iter;
		size_type val_count = goto_next_larger(next);
		do_sort_values(iter, next, val_count, compVal);
		return val_count;
	}
	template<class CompValueT>
	void sort_values(CompValueT compVal)
	{
		BOOST_STATIC_ASSERT(!is_unique_key_t::value); // unique key has no sort
		std::vector<ValueT> temp;
		std::vector<iterator> vptr;
		for (iterator iter = begin(); iter != end(); )
		{
			iterator ilow = iter;
			size_type val_count = goto_next_larger(iter);
			do_sort_values(ilow, iter, val_count, compVal);
		}
	}
	//@}

//////////////////////////////////////////////////////////////////////////
/// @name non-const operations @{
#define TERARK_M_const const
#define TERARK_M_iterator const_iterator
#define TERARK_M_reverse_iterator const_reverse_iterator
#include "packed_table_mf.hpp"
/// @}

/// @name const operations @{
#define TERARK_M_const
#define TERARK_M_iterator iterator
#define TERARK_M_reverse_iterator reverse_iterator
#include "packed_table_mf.hpp"
/// @}
//////////////////////////////////////////////////////////////////////////

private:
	template<class ThisT, class IterT>
	static size_type goto_next_larger_aux(ThisT* self, IterT& iter, boost:: true_type)
	{
		assert(self->end() != iter);
		++iter;
		return 1;
	}
	template<class ThisT, class IterT>
	static size_type goto_next_larger_aux(ThisT* self, IterT& iter, boost::mpl::false_)
	{
		assert(self->end() != iter);

//#define TERARK_PACKED_TABLE_COUNT_PREV_EQUAL
#ifdef TERARK_PACKED_TABLE_COUNT_PREV_EQUAL
		size_type count = 0;
		typename IterT::index_iter_t prev = iter.m_iter;
		for (;; --prev) // count 'iter' self
		{
			if (EffectiveCompare(CompID(), *prev, *iter.m_iter) == 0)
				++count;
			else break;

			if (self->m_index.begin() == prev) break;
		}
#else
#if defined(_DEBUG) || defined(DEBUG) || !defined(NDEBUG)
		typename IterT::index_iter_t prev = iter.m_iter;
		if (self->m_index.begin() != iter.m_iter)
		{
			// iter must be the first element of its values!!
			--prev;
			bool bless = self->m_index.value_comp()(*prev, *iter.m_iter);
			assert(bless);
		}
#	endif // DEBUG
		size_type count = 1; // 1 means 'iter' self has been counted
#endif //TERARK_PACKED_TABLE_COUNT_PREV_EQUAL

		typename IterT::index_iter_t next = iter.m_iter;
		for (++next; self->m_index.end() != next; ++next) // do not count 'iter' self
		{
			if (EffectiveCompare(CompID(), *iter.m_iter, *next) == 0)
				++count;
			else break;
		}
		assert(std::distance(iter.m_iter, next) == count);
		assert(self->m_index.upper_bound(self->raw_key(iter)) == next);
		iter.m_iter = next;
		return count;
	}

	value_type& valueFromKey(const KeyT& key, boost::mpl::true_ isUniqueKey)
	{
		iterator iter = m_index.find(key);
		if (m_index.end() == iter) {
			iter.m_iter = m_index.insert(node_t(m_key_pool.make_key(key), value_type())).first;
		}
		return const_cast<value_type&>(*iter);
	}
	value_type& valueFromKey(KeyT key, boost::mpl::false_ isUniqueKey)
	{
		index_t::NoneUniqueKeyHasNotOperatorBracket(); // raise compile error!!
		return const_cast<value_type&>(*begin());
	}

	iter_bool_t insert_aux(iterator hint, const key_type& key, const value_type& val, boost::mpl::true_ isUniqueKey)
	{
		std::pair<typename index_t::iterator, bool>
		//	ib = m_index.insert(hint.m_iter, node_t(m_key_pool.make_key(key), val));
			ib = m_index.insert(node_t(m_key_pool.make_key(key), val));
		if (!ib.second)
			m_key_pool.unmake_key(key);

		return iter_bool_t(iterator(ib.first), ib.second);
	}

	iterator insert_aux(iterator hint, const key_type& key, const value_type& val, boost::mpl::false_ isUniqueKey)
	{
		iterator iter = find(key);
		if (end() == iter) {
			iter = m_index.insert(iter.m_iter, node_t(m_key_pool.make_key(key), val));
		} else
			iter = m_index.insert(hint.m_iter, *iter.m_iter);

		return iter;
	}

	void insert_aux(const PackedTable& other, const_iterator first, const_iterator last, boost::mpl::true_)
	{
		iterator hint = end();
		for (const_iterator iter = first; iter != last; ++iter)
		{
			hint = insert(hint, other.raw_key(iter), *iter).first;
		}
	}

	void insert_aux(const PackedTable& other, const_iterator first, const_iterator last, boost::mpl::false_)
	{
		for (const_iterator iter = first; iter != last;)
		{
			const_iterator ilow = iter;
			size_t count = other.goto_next_larger(iter);
		//	insert(other.raw_key(ilow), ilow, iter, count);
			insert(other.raw_key(ilow), ilow, iter);
		}
	}

public:
	value_type& operator[](const key_type& key) { return valueFromKey(key, is_unique_key_t()); }

	insert_return_t insert(const std::pair<key_type, value_type>& kv)
	{
		return insert_aux(m_index.end(), kv.first, kv.second, is_unique_key_t());
	}

	insert_return_t insert(iterator pos, const std::pair<key_type, value_type>& kv)
	{
		return insert_aux(pos, kv.first, kv.second, is_unique_key_t());
	}

	insert_return_t insert(const key_type& key, const value_type& val)
	{
		return insert_aux(m_index.end(), key, val, is_unique_key_t());
	}
	insert_return_t insert(iterator hint, const key_type& key, const value_type& val)
	{
		return insert_aux(hint, key, val, is_unique_key_t());
	}

	void insert(const PackedTable& other, const_iterator first, const_iterator last)
	{
		return insert_aux(other, first, last, is_unique_key_t());
	}
	void insert(const PackedTable& other)
	{
		insert(other, other.begin(), other.end());
	}

	template<class IterT>
	void insert(const key_type& key, const IterT& first, const IterT& last)
	{
		BOOST_STATIC_ASSERT(!is_unique_key_t::value);

		if (first == last) return;
		IterT iter = first;
		typename node_t::key_id_t keyID;
		typename index_t::iterator hint = m_index.find(key);
		if (m_index.end() == hint) {
			keyID = m_key_pool.make_key(key);
			hint = m_index.insert(node_t(keyID, *iter)).first;
			++iter;
		} else {
			keyID = (*hint).key_id();
		}
		for (; iter != last; ++iter)
		{
			hint = m_index.insert(hint, node_t(keyID, *iter));
		}
	}

	/**
	 @brief now, only used for MultiPackedTable
	 */
	typename index_t::iterator insert_node(const node_t& node)
	{
		return m_index.insert(node).first;
	}

	/**
	 @brief dummy, for BiWayTable
	 */
	void set_unzip_prefer(unzip_prefer uzp) {}

	template<class DataInput > void load_key_pool(DataInput &  input) {  input >> m_key_pool; }
	template<class DataOutput> void save_key_pool(DataOutput& output) const { output << m_key_pool; }

	template<class DataInput > void dump_load(DataInput & input)
	{
		m_index.clear();

		uint32_t count;
		input >> count >> m_key_pool;

		for (uint32_t i = 0; i != count; ++i)
		{
			node_t node;
			input >> node;
			m_index.insert(node);
		}
	}
	template<class DataOutput> void dump_save(DataOutput& output) const
	{
		uint32_t count = size();
		output << count << m_key_pool;
		for (typename index_t::const_iterator iter = m_index.begin(); iter != m_index.end(); ++iter)
		{
			output << *iter;
		}
	}
	DATA_IO_REG_DUMP_LOAD_SAVE(PackedTable)

	void init(uint32_t maxPoolSize)
	{
		m_key_pool.init(maxPoolSize);
	}

	void setMaxPoolSize(uint32_t maxPoolSize)
	{
		m_key_pool.setMaxPoolSize(maxPoolSize);
	}

protected:
	//! @{
	//! @brief 支持序列化 serialization
	template<class DataInput > void load(DataInput & input) { dump_load(input); }
	template<class DataOutput> void save(DataOutput& output) const { dump_save(output); }

	DATA_IO_REG_LOAD_SAVE(PackedTable)
	//! @}
protected:
	index_t    m_index;
	key_pool_t m_key_pool;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template<class KeyT, class ValueT, class CompareT>
class MultiPackedTable; // forward declaration

template<class KeyT, class ValueT, class CompareT>
class MultiPackedTable_ConstIterator; // forward declaration

/**
 @brief 迭代器定义
 */
template<class KeyT, class ValueT, class CompareT>
class MultiPackedTable_Iterator :
	public boost::bidirectional_iterator_helper<
				MultiPackedTable_Iterator<KeyT, ValueT, CompareT>,
				ValueT,
				std::ptrdiff_t,
				ValueT*,
				ValueT&>
{
	friend class MultiPackedTable<KeyT, ValueT, CompareT>;
	friend class MultiPackedTable_ConstIterator<KeyT, ValueT, CompareT>;
public:
	typedef MultiPackedTable<KeyT, ValueT, CompareT> table_t;
	typedef typename IndexNode<ValueT>::type node_t;
	typedef typename table_t::index_t		 index_t;
	typedef typename index_t::iterator		 index_iter_t;
	typedef typename table_t::val_vec_t		 val_vec_t;

private:
	index_iter_t m_iter;
	int m_nth;

public:
	MultiPackedTable_Iterator(index_iter_t iter, int nth = 0) : m_iter(iter), m_nth(nth) {}
	MultiPackedTable_Iterator() : m_nth(-1) {}

	ValueT& operator*() const { return (*m_iter)[m_nth]; }

	MultiPackedTable_Iterator& operator++()
	{
		++m_nth;
		if ((*m_iter).size() == m_nth) {
			++m_iter;
			m_nth = 0;
		}
		return *this;
	}
	MultiPackedTable_Iterator& operator--()
	{
		if (0 == m_nth) {
			--m_iter;
			m_nth = (*m_iter).size() - 1;
		} else
			--m_nth;
		return *this;
	}

	friend bool operator==(const MultiPackedTable_Iterator& x, const MultiPackedTable_Iterator& y)
	{
		return x.m_iter == y.m_iter && x.m_nth == y.m_nth;
	}
};

/**
 @brief 常量迭代器定义
 */
template<class KeyT, class ValueT, class CompareT>
class MultiPackedTable_ConstIterator :
	public boost::bidirectional_iterator_helper<
				MultiPackedTable_ConstIterator<KeyT, ValueT, CompareT>,
				ValueT,
				std::ptrdiff_t,
				ValueT*,
				ValueT&>
{
	friend class MultiPackedTable<KeyT, ValueT, CompareT>;

public:
	typedef MultiPackedTable<KeyT, ValueT, CompareT> table_t;
	typedef typename IndexNode<ValueT>::type node_t;
	typedef typename table_t::index_t		 index_t;
	typedef typename index_t::const_iterator index_iter_t;
	typedef typename table_t::val_vec_t		 val_vec_t;

private:
	index_iter_t m_iter;
	int m_nth;

public:
	MultiPackedTable_ConstIterator(index_iter_t iter, int nth = 0)
		: m_iter(iter), m_nth(nth) {}
	MultiPackedTable_ConstIterator() : m_nth(-1) {}

	MultiPackedTable_ConstIterator(MultiPackedTable_Iterator<KeyT, ValueT, CompareT> iter)
		: m_iter(iter.m_iter), m_nth(iter.m_nth) { }

	const ValueT& operator*() const { return (*m_iter)[m_nth]; }

	MultiPackedTable_ConstIterator& operator++()
	{
		++m_nth;
		if ((*m_iter).size() == m_nth) {
			++m_iter;
			m_nth = 0;
		}
		return *this;
	}
	MultiPackedTable_ConstIterator& operator--()
	{
		if (0 == m_nth) {
			--m_iter;
			m_nth = (*m_iter).size() - 1;
		} else
			--m_nth;
		return *this;
	}

	friend bool operator==(const MultiPackedTable_ConstIterator& x, const MultiPackedTable_ConstIterator& y)
	{
		return x.m_iter == y.m_iter && x.m_nth == y.m_nth;
	}
};

template<class KeyT, class ValueT, class CompareT>
class MultiPackedTable
{
public:
	typedef std::vector<ValueT> val_vec_t;
	typedef PackedTable<KeyT, val_vec_t, CompareT, true> index_t;

	typedef Table_dump_header         dump_header;
	typedef typename index_t::node_t		node_t;
	typedef typename index_t::CompID		CompID;
	typedef typename index_t::key_pool_t	key_pool_t;
	typedef typename key_pool_t::raw_key_t  raw_key_t;

	typedef typename index_t::size_type	size_type;
	typedef CompareT					compare_t;
	typedef KeyT						key_type;
	typedef ValueT						value_type;
	typedef ValueT&						reference;

	typedef MultiPackedTable_Iterator<KeyT, ValueT, CompareT>			 iterator;
	typedef MultiPackedTable_ConstIterator<KeyT, ValueT, CompareT> const_iterator;

	typedef boost::reverse_iterator<iterator>		reverse_iterator;
	typedef boost::reverse_iterator<const_iterator> const_reverse_iterator;

	typedef std::pair<const_iterator, const_iterator> const_range_t;
	typedef std::pair<iterator, iterator> range_t;
	typedef std::pair<iterator, bool> iter_bool_t;

	typedef boost::integral_constant<bool, false> is_unique_key_t;
	typedef iterator insert_return_t;

private:
	index_t m_index;
	size_t  m_valCount;

public:
	explicit MultiPackedTable(uint32_t maxKeyPool = TERARK_IF_DEBUG(8*1024, 2*1024*1024),
							  CompareT comp = CompareT())
		: m_index(maxKeyPool, comp), m_valCount(0)
	{
	}

	iterator insert(const std::pair<key_type, value_type>& kv)
	{
		return insert(kv.first, kv.second);
	}

	iterator insert(iterator pos, const std::pair<key_type, value_type>& kv)
	{
		return insert(kv.first, kv.second);
	}

	iterator insert(const key_type& key, const value_type& val)
	{
		typename index_t::iterator found = m_index.find(key);
		if (m_index.end() == found)
		{
			found = m_index.insert(key, val_vec_t()).first;
		}
		(*found).push_back(val);
		m_valCount++;
		return iterator(found, (*found).size() - 1);
	}

	void insert(const MultiPackedTable& other, const_iterator first, const_iterator last)
	{
		for (const_iterator i = first; i != last; ++i)
		{
			insert(other.raw_key(i), *i);
		}
	}
	void insert(const MultiPackedTable& other)
	{
		insert(other, other.begin(), other.end());
	}

	template<bool IsUniqueKey, class IterT>
	void insert(const PackedTable<KeyT, ValueT, CompareT, IsUniqueKey>& other, IterT first, IterT last)
	{
		for (IterT i = first; i != last; ++i)
		{
			insert(other.raw_key(i), *i);
		}
	}
	template<bool IsUniqueKey>
	void insert(const PackedTable<KeyT, ValueT, CompareT, IsUniqueKey>& other)
	{
		insert(other, other.begin(), other.end());
	}

	template<class IterT>
	void insert(const key_type& key, const IterT& first, const IterT& last)
	{
		typename index_t::iterator found = m_index.find(key);
		if (m_index.end() == found)
		{
			found = m_index.insert(key, val_vec_t()).first;
		}
		size_t old_size = (*found).size();
		(*found).insert((*found).end(), first, last);
		m_valCount += (*found).size() - old_size;
	}

	void clear()
	{
		m_index.clear();
		m_valCount = 0;
	}

public:
	/**
	 @{
	 @brief 为 iter 指向的 key 关联的 values 排序

	 @return 返回排序的 value_count
	 @note iter 必须指向 key-values 中的第一个 value
	 */
	template<class CompValueT>
	size_t sort_values(iterator iter, CompValueT compVal)
	{
		assert(iter != end());

		std::sort((*iter.m_iter).begin(),
				  (*iter.m_iter).end(), compVal);
		return (*iter.m_iter).size();
	}
	template<class CompValueT>
	void sort_values(CompValueT compVal)
	{
		for (typename index_t::iterator iter = m_index.begin(); iter != m_index.end(); ++iter)
		{
			std::sort((*iter).begin(), (*iter).end(), compVal);
		}
	}
	//@}

	void init(uint32_t maxPoolSize)
	{
		m_index.init(maxPoolSize);
	}
	void setMaxPoolSize(uint32_t maxPoolSize)
	{
		m_index.setMaxPoolSize(maxPoolSize);
	}

//////////////////////////////////////////////////////////////////////////
/// @name non-const operations @{
#define TERARK_M_const const
#define TERARK_M_iterator const_iterator
#define TERARK_M_reverse_iterator const_reverse_iterator
#include "packed_table_mf_multi.hpp"
/// @}

/// @name const operations @{
#define TERARK_M_const
#define TERARK_M_iterator iterator
#define TERARK_M_reverse_iterator reverse_iterator
#include "packed_table_mf_multi.hpp"
/// @}
//////////////////////////////////////////////////////////////////////////

	/**
	 @brief dummy, for BiWayTable
	 */
	void set_unzip_prefer(unzip_prefer uzp) {}

	template<class DataInput > void dump_load(DataInput & input)
	{
		clear();
		uint32_t keyCount;
		input >> keyCount;
		m_index.load_key_pool(input);
		for (uint32_t i = 0; i != keyCount; ++i)
		{
			typename index_t::node_t node;
			node.load_key(input);
			uint32_t valSize;
			input >> valSize;
			typename index_t::node_t& node2 =
				const_cast<typename index_t::node_t&>(*m_index.insert_node(node));
			node2.val().resize(valSize);
			input.ensureRead(&*node2.val().begin(), sizeof(ValueT)*valSize);
			m_valCount += valSize;
		}
	}
	template<class DataOutput> void dump_save(DataOutput& output) const
	{
		uint32_t keyCount = m_index.size();
		output << keyCount;
		m_index.save_key_pool(output);
		size_t valCount2 = 0;
		for (typename index_t::const_iterator iter = m_index.begin(); iter != m_index.end(); ++iter)
		{
			const typename index_t::node_t& node = iter.get_node();
			node.save_key(output);
			uint32_t vals = node.val().size();
			output << vals;
			output.ensureWrite(&*node.val().begin(), sizeof(ValueT)*vals);
			valCount2 += vals;
		}
		TERARK_RT_assert(valCount2 == m_valCount, std::logic_error);
	}
	DATA_IO_REG_DUMP_LOAD_SAVE(MultiPackedTable)

protected:
	//! @{
	//! @brief 支持序列化 serialization
	template<class DataInput > void load(DataInput & input) { dump_load(input); }
	template<class DataOutput> void save(DataOutput& output) const { dump_load(output); }

	DATA_IO_REG_LOAD_SAVE(MultiPackedTable)
	//! @}
};


} } // namespace terark::prefix_zip


#endif // __packed_table_h__


