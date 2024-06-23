/* vim: set tabstop=4 : */
#ifndef __terark_sorted_table_h__
#define __terark_sorted_table_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

#include <vector>
#include <functional>
#include <algorithm>

#include <terark/io/DataIO.hpp>
#include "ext_index.hpp"
#include "key_pool.hpp"
#include "dump_header.hpp"

namespace terark { namespace prefix_zip {

namespace tab_serialize_type
{
	BOOST_STATIC_CONSTANT(uint32_t, dump  = 1);
	BOOST_STATIC_CONSTANT(uint32_t, merge = 2);
}

template<class KeyT, class ValueT, class KeyCompareT>
class SortedTable;

template<class KeyT, class ValueT, class KeyCompareT>
class SortedTable_ConstIterator;

template<class KeyT, class ValueT, class KeyCompareT,
		 class IndexIter,
		 class BaseIteratorDef,
		 class SelfReflected
>
class SortedTable_IteratorBase : public BaseIteratorDef
{
protected:
	IndexIter m_iter;

public:
	typedef IndexIter index_iter_t;
	typedef KeyT key_type;
	typedef typename BaseIteratorDef::value_type value_type;
	typedef typename BaseIteratorDef::reference  reference;
	typedef typename BaseIteratorDef::difference_type difference_type;

	SortedTable_IteratorBase(IndexIter iter) : m_iter(iter) {}
	SortedTable_IteratorBase() {}

	reference operator*() const { return (*m_iter).val(); }

	SelfReflected& operator++() { ++m_iter; return static_cast<SelfReflected&>(*this); }
	SelfReflected& operator--() { --m_iter; return static_cast<SelfReflected&>(*this); }

	SelfReflected& operator+=(difference_type diff)
	{
		m_iter += diff;
		return static_cast<SelfReflected&>(*this);
	}
	SelfReflected& operator-=(difference_type diff)
	{
		m_iter -= diff;
		return static_cast<SelfReflected&>(*this);
	}

	friend bool operator==(const SelfReflected& x, const SelfReflected& y)
	{
		return x.m_iter == y.m_iter;
	}

	friend difference_type operator-(const SelfReflected& x, const SelfReflected& y)
	{
		return x.m_iter - y.m_iter;
	}
	friend bool operator<(const SelfReflected& x, const SelfReflected& y)
	{
		return x.m_iter < y.m_iter;
	}
};
#define TERARK_SORTED_TABLE_ITERATOR_BASE(IterClass, ConstVal, IterType) \
	SortedTable_IteratorBase<		\
		KeyT, ValueT, KeyCompareT,	\
		typename std::vector<KeyValueNode<KeyT, ValueT, KeyCompareT> >::IterType, \
		boost::random_access_iterator_helper<		\
			IterClass<KeyT, ValueT, KeyCompareT>,	\
			ConstVal,		\
			ptrdiff_t,		\
			ConstVal*,		\
			ConstVal&		\
		>,					\
		IterClass<KeyT, ValueT, KeyCompareT> >
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

template<class KeyT, class ValueT, class KeyCompareT>
class SortedTable_Iterator :
	public  TERARK_SORTED_TABLE_ITERATOR_BASE(SortedTable_Iterator, ValueT, iterator)
{
	typedef TERARK_SORTED_TABLE_ITERATOR_BASE(SortedTable_Iterator, ValueT, iterator) super;

	friend class SortedTable<KeyT, ValueT, KeyCompareT>;
	friend class SortedTable_ConstIterator<KeyT, ValueT, KeyCompareT>;

public:
	SortedTable_Iterator(typename super::index_iter_t iter)
		: super(iter) {}
	SortedTable_Iterator() {}
};

template<class KeyT, class ValueT, class KeyCompareT>
class SortedTable_ConstIterator :
	public  TERARK_SORTED_TABLE_ITERATOR_BASE(SortedTable_ConstIterator, const ValueT, const_iterator)
{
	typedef TERARK_SORTED_TABLE_ITERATOR_BASE(SortedTable_ConstIterator, const ValueT, const_iterator) super;

	friend class SortedTable<KeyT, ValueT, KeyCompareT>;

public:
	SortedTable_ConstIterator(typename super::index_iter_t iter)
		: super(iter) {}

	SortedTable_ConstIterator() {}
	SortedTable_ConstIterator(SortedTable_Iterator<KeyT, ValueT, KeyCompareT> iter)
		: super(iter.m_iter) { }
};

/**
 @brief 容纳任意 KeyType 的映射表，Key 可重复

 相应于 KeyType 是 std::string 的 SortedTable ，接口和 SortedTable 比较相似，
 但 Key 和 Value 是共存在一个结点中的，key_pool 只是一个伪 pool
  -# 可以做为 MultiWayTable 的固定索引
  -# 可以做为 BiWayTable 的基本容器 (BaseTableT)
 */
template<class KeyT, class ValueT, class KeyCompareT>
class SortedTable
{
	typedef KeyValueNode<KeyT, ValueT, KeyCompareT> node_t;
	typedef KeyPool<KeyT> key_pool_t;
	typedef std::vector<node_t>   index_t;

	key_pool_t  m_key_pool;
	index_t		m_index;
	KeyCompareT	m_comp;

	template<class CompatibleKey, class CompatibleCompare>
	struct CompareNodeKey : private CompatibleCompare
	{
		CompareNodeKey(CompatibleCompare comp) : CompatibleCompare(comp) {}

		bool operator()(const node_t& x, const CompatibleKey& y) const
		{
			return CompatibleCompare::operator()(x.key(), y);
		}
		bool operator()(const CompatibleKey& x, const node_t& y) const
		{
			return CompatibleCompare::operator()(x, y.key());
		}

		bool operator()(const CompatibleKey& x, const CompatibleKey& y) const
		{
			return CompatibleCompare::operator()(x, y);
		}
		bool operator()(const node_t& x, const node_t& y) const
		{
			return CompatibleCompare::operator()(x.key(), y.key());
		}
	};

	template<class ValueCompare>
	struct CompareNodeValue : private ValueCompare
	{
		CompareNodeValue(ValueCompare comp) : ValueCompare(comp) {}

		bool operator()(const node_t& x, const node_t& y) const
		{
			return ValueCompare::operator()(x.val(), y.val());
		}
	};

public:
	typedef Table_dump_header dump_header;

	typedef typename std::vector<node_t>::size_type size_type;
	typedef typename std::vector<node_t>::difference_type difference_type;
	typedef KeyT   raw_key_t;
	typedef KeyT   key_type;
	typedef ValueT value_type;
	typedef SortedTable_Iterator     <KeyT, ValueT, KeyCompareT>       iterator;
	typedef SortedTable_ConstIterator<KeyT, ValueT, KeyCompareT> const_iterator;
	typedef boost::reverse_iterator<iterator>		reverse_iterator;
	typedef boost::reverse_iterator<const_iterator> const_reverse_iterator;

	typedef std::pair<iterator, iterator> range_t;
	typedef std::pair<const_iterator, const_iterator> const_range_t;
	typedef KeyCompareT key_compare;
	typedef KeyCompareT compare_t;

	explicit SortedTable(uint32_t keyPoolSize = TERARK_IF_DEBUG(8*1024, 2*1024*1024),
						 const key_compare& comp = key_compare())
		: m_comp(comp), m_key_pool(keyPoolSize)
	{
		m_index.push_back(node_t());
	}

	void swap(SortedTable& that)
	{
		std::swap(m_comp, that.m_comp);
		std::swap(m_key_pool, that.m_key_pool);
		m_index.swap(that.m_index);
	}
	friend void swap(SortedTable& left, SortedTable& right) { left.swap(right);	}

	void clear()
	{
		m_key_pool.reset();
		index_t().swap(m_index);
	//	m_index.reserve(m_key_pool.total_size() / sizeof(KeyT) + 1);
		m_index.push_back(node_t());
	}

	void init(uint32_t keyPoolSize)
	{
		m_key_pool.init(keyPoolSize);
		m_index.clear();
		m_index.reserve(m_key_pool.total_size() / sizeof(key_type) + 1);
		m_index.push_back(node_t());
	}

	void trim_extra()
	{
		if (double(m_index.size()) < double(m_index.capacity()) * 0.8)
		{
			std::vector<node_t>(m_index).swap(m_index);
		}
	}

	void key(const_iterator iter, key_type& xkey) const
	{
		assert(iter >= begin());
		assert(iter <  end());
		xkey = iter.m_iter->key();
	}
	const key_type& key(const_iterator iter) const
	{
		assert(iter >= begin());
		assert(iter <  end());
		return iter.m_iter->key();
	}
	raw_key_t raw_key(const_iterator iter) const
	{
		assert(iter >= begin());
		assert(iter <  end());
		return iter.m_iter->key();
	}

	const key_type& lastKey() const
	{
		assert(m_index.size() > 1);
		return (m_index.end()-2)->key();
	}

	template<class EnumT>
	void set_unzip_prefer(EnumT uzp) {} // ignore

	size_t unzip_key_size() const { return m_key_pool.used_size(); }

	uint32_t  used_pool_size() const { return m_key_pool.used_size(); }
	uint32_t total_pool_size() const { return m_key_pool.total_size(); }

	size_t  used_mem_size() const { return sizeof(node_t) * m_index.size(); }
	size_t total_mem_size() const { return sizeof(node_t) * m_index.capacity(); }

	static void check_key(const key_type& xkey) {} // do nothing...

	void push_back(const key_type& xkey, const value_type& val)
	{
		assert(empty() || !m_comp(xkey, lastKey()));

		m_index.back() = node_t(m_key_pool.make_key(xkey), val);
	//	m_index.push_back(m_index.back());
		m_index.push_back(node_t());
	}
	template<class IterT>
	void push_back(const key_type& xkey, IterT first, IterT last)
	{
		assert(empty() || !m_comp(xkey, lastKey()));

		m_index.pop_back();
		if (first != last)
		{	//! pretend to make a key, @see KeyPool::make_key
			m_key_pool.make_key(xkey);
		}
		for (; first != last; ++first)
			m_index.push_back(node_t(xkey, *first));
	//	m_index.push_back(m_index.back()); // copy back as last dummy
		m_index.push_back(node_t());
	}
	template<class IterT>
	void push_back(const key_type& xkey, IterT first, IterT last, size_type count)
	{
		assert(empty() || !m_comp(xkey, lastKey()));
		size_type old_size = size();
		push_back(xkey, first, last);
		assert(old_size + count == size());
	}
	void pop_back_n(size_type n)
	{
		assert(size() >= n);
		m_index.erase(m_index.end() - n, m_index.end());
		m_key_pool.seek(size() * sizeof(KeyT));
	}

///////////////////////////////////////////////////////////////////////////////////////////////
#define TERARK_PZ_SEARCH_FUN_ARG_LIST_II  first.m_iter, last.m_iter, xkey, CompareNodeKey<key_type, CompatibleCompare>(comp)
#define TERARK_PZ_SEARCH_FUN_ARG_LIST_BE_1  m_index.begin(), m_index.end()-1, xkey, CompareNodeKey<key_type, CompatibleCompare>(comp)
#define TERARK_PZ_SEARCH_FUN_ARG_LIST_BE_0  m_index.begin(), m_index.end()-1, xkey, CompareNodeKey<key_type, key_compare>(m_comp)
#define TERARK_PZ_FUN(func)   std::func
#include "sorted_table_mf_1.hpp"
///////////////////////////////////////////////////////////////////////////////////////////////

// 	size_t key_count() const
// 	{
// 		typename index_t::const_iterator iter = m_index.begin();
// 		size_t sz = 0;
// 		while (iter != m_index.end() - 1)
// 		{
// 			iter = upper_bound_near(iter, m_index.end()-1, *iter,
// 				CompareNodeKey<key_type, key_compare>(m_comp));
// 			sz++;
// 		}
// 		return sz;
// 	}

	size_type key_count() const { return m_key_pool.key_count(); }

//////////////////////////////////////////////////////////////////////////
	bool last_is_zipped() const { return true; }

	template<class Input>
	void load_data(Input& input, size_type count) const
	{
		node_t node;
		for (size_type i = 0; i != count; ++i)
		{
			input >> node;
			m_index.push_back(node);
		}
		m_index.push_back(node);
	}
	template<class Output>
	void save_data(Output& output) const
	{
		for (typename index_t::const_iterator iter = m_index.begin(); iter != m_index.end()-1; ++iter)
		{
			output << *iter;
		}
	}

	template<class Input>
	void load(Input& input, uint32_t version)
	{
		dump_header header;
		input >> header;
		m_index.reserve(header.valCount + 1);
		load_data(m_index);
	}
	template<class Output>
	void save(Output& output, uint32_t version) const
	{
		dump_header header(*this);
		output << header;
		save_data(output);
	}
	template<class Input>
	void dump_load(Input& input)
	{
		dump_header header;
		input >> header;
		m_index.resize(header.valCount + 1);
		m_key_pool.init(header.valCount * sizeof(KeyT));
		if (header.valCount)
			input.ensureRead(&*m_index.begin(), sizeof(node_t)*header.valCount);
	}
	template<class Output>
	void dump_save(Output& output) const
	{
		dump_header header(*this);
		output << header;
		if (header.valCount)
			output.ensureWrite(&*m_index.begin(), sizeof(node_t)*header.valCount);
	}
	DATA_IO_REG_DUMP_LOAD_SAVE(SortedTable)

	/**
	 @name 加载与存储合并的段

	 - 在合并多个索引（段）时，会使用一个比较小的临时的段来存储合并后的数据，
	 - 对于不同的 key，这些合并后的数据是按 key 排序的，
	 - 对于同一个 key 的不同 value，这些 values 是按插入的次序排序的，
	 - 因此，这个临时段存储的数据在逻辑上是多个索引合并后产生的大段中一段连续的数据。
	 - 之所以用这个临时段，是为了避免合并成一个大段时需要大量的内存。

	 @note - 因为这存储/加载实际上相当于内存 dump，因此即使使用了 PortableDataInput/PortableDataOutput
	         对于不同的平台和编译器，也不能保证产生的数据文件互相兼容，特别是 BigEndian/LittleEndian
		   - 要产生格式兼容的数据文件，必须使用默认的 serialization，即 operator& operator<< operator>>
	 */
	/**
	 @brief load one segment, this segment maybe a continuous part of a large table

	 @return value count in this segment

	  存储 segment 时，直接调用 dump_save ，把整个表存为一个 segment
	 */
	template<class Input >
	void dump_load_segment(Input& input, dump_header& loading, const dump_header& loaded)
	{
		uint32_t count;
		input >> count;
		if (count)
			input.ensureRead(&*m_index.begin() + loading.valCount, count*sizeof(node_t));
		loading.valCount += count;
	}
	//! 准备 dump_load_merged, 如预分配空间等
	void prepair_dump_load_merged(const dump_header& loaded)
	{
		m_key_pool.init(sizeof(key_type)*loaded.valCount);
		m_index.resize(loaded.valCount + 1);
	}
	//! 完成 dump_load_merged, 进行收尾工作
	template<class Input>
	void complete_dump_load_merged(Input& input, dump_header& loading, const dump_header& loaded)
	{
		if (this->size() != loading.valCount)
		{
			string_appender<> oss;
			oss << "data format error, count[saved=" << this->size() << ", actual=" << loading.valCount << "]";
			throw DataFormatException(oss.str());
		}
		trim_extra();
	}
	/**
	 @brief 将内容存储成一个段
	 */
	template<class Output>
	void dump_save_segment(Output& output, dump_header& header) const
	{
		uint32_t count = this->size();
		output << count;
		if (count)
			output.ensureWrite(&*m_index.begin(), sizeof(node_t)*count);
		header.valCount += count;
	}

	DATA_IO_REG_LOAD_SAVE_V(SortedTable, 1)
};

template<class KeyT, class ValueT, class KeyCompareT, class IndexElement>
struct ExtIndexType<SortedTable<KeyT, ValueT, KeyCompareT>, IndexElement>
{
	typedef SortedTable<KeyT, IndexElement, KeyCompareT> index_t;
	typedef SortedTable<KeyT, ValueT,       KeyCompareT> multi_t;
};


} } // namespace terark::prefix_zip

#endif // __terark_sorted_table_h__
