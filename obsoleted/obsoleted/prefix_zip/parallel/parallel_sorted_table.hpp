#ifndef __terark_parallel_sorted_table_h__
#define __terark_parallel_sorted_table_h__

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

/**
 @brief 容纳任意 KeyType 的映射表，Key 可重复

 相应于 KeyType 是 std::string 的 parallel_sorted_table ，接口和 parallel_sorted_table 比较相似，
 但 Key 和 Value 是共存在一个结点中的，key_pool 只是一个伪 pool
  -# 可以做为 MultiWayTable 的固定索引
 */
template<class KeyT, class KeyCompareT>
class parallel_sorted_table : public std::vector<KeyT>
{
	typedef KeyPool<KeyT> key_pool_t;

	key_pool_t  m_key_pool;
	KeyCompareT	m_comp;

public:
	typedef Table_dump_header dump_header;

	typedef typename std::vector<KeyT>::size_type size_type;
	typedef typename std::vector<KeyT>::difference_type difference_type;
	typedef KeyT   raw_key_t;
	typedef KeyT   key_type;

	typedef std::pair<iterator, iterator> range_t;
	typedef std::pair<const_iterator, const_iterator> const_range_t;
	typedef KeyCompareT key_compare;
	typedef KeyCompareT compare_t;

	explicit parallel_sorted_table(uint32_t keyPoolSize = TERARK_IF_DEBUG(8*1024, 2*1024*1024),
						 const key_compare& comp = key_compare())
		: m_comp(comp), m_key_pool(keyPoolSize)
	{
	}

	void swap(parallel_sorted_table& that)
	{
		std::swap(m_comp, that.m_comp);
		std::swap(m_key_pool, that.m_key_pool);
		m_keytab.swap(that.m_keytab);
	}
	friend void swap(parallel_sorted_table& left, parallel_sorted_table& right) { left.swap(right);	}

	void clear()
	{
		m_key_pool.reset();
		m_keytab.clear();
	}

	void init(uint32_t keyPoolSize)
	{
		m_key_pool.init(keyPoolSize);
		m_keytab.clear();
		size_type newCapacity = m_key_pool.total_size() / sizeof(key_type);
		m_keytab.reserve(newCapacity);
	}

	void trim_extra()
	{
		if (double(m_keytab.size()) < double(m_keytab.capacity()) * 0.8)
		{
			key_tab_t(m_keytab).swap(m_keytab);
		}
	}

	void key(const_iterator iter, key_type& xkey) const
	{
		assert(iter >= begin());
		assert(iter <  end());
		xkey = *iter;
	}
	const key_type& key(const_iterator iter) const
	{
		assert(iter >= begin());
		assert(iter <  end());
		return
	}
	raw_key_t raw_key(const_iterator iter) const
	{
		assert(iter >= begin());
		assert(iter <  end());
		return m_keytab[iter - m_data.begin()];
	}

	const key_type& lastKey() const
	{
		assert(m_keytab.size() >= 1);
		return m_keytab.back();
	}

	template<class EnumT>
	void set_unzip_prefer(EnumT uzp) {} // ignore

	size_t unzip_key_size() const { return m_key_pool.used_size(); }

	uint32_t  used_pool_size() const { return m_key_pool.used_size(); }
	uint32_t total_pool_size() const { return m_key_pool.total_size(); }

	size_t  used_mem_size() const { return sizeof(node_t) * m_keytab.size(); }
	size_t total_mem_size() const { return sizeof(node_t) * m_keytab.capacity(); }

	static void check_key(const key_type& xkey) {} // do nothing...

	void push_back(const key_type& xkey, const value_type& val)
	{
		assert(empty() || !m_comp(xkey, lastKey()));

		m_keytab.push_back(xkey);
		m_data.push_back(val);
	}
	template<class IterT>
	void push_back(const key_type& xkey, IterT first, IterT last)
	{
		assert(empty() || !m_comp(xkey, lastKey()));

		if (first != last)
		{	//! pretend to make a key, @see KeyPool::make_key
			m_key_pool.make_key(xkey);
		}
		for (; first != last; ++first)
		{
			m_keytab.push_back(xkey);
			m_data.push_back(*first);
		}
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
		m_keytab.erase(m_keytab.end() - n, m_keytab.end());
		m_key_pool.seek(size() * sizeof(KeyT));
	}

///////////////////////////////////////////////////////////////////////////////////////////////
#define TERARK_PZ_SEARCH_FUN_ARG_LIST_II  first.m_iter, last.m_iter, xkey, CompareNodeKey<key_type, CompatibleCompare>(comp)
#define TERARK_PZ_SEARCH_FUN_ARG_LIST_BE_1  m_keytab.begin(), m_keytab.end(), xkey, CompareNodeKey<key_type, CompatibleCompare>(comp)
#define TERARK_PZ_SEARCH_FUN_ARG_LIST_BE_0  m_keytab.begin(), m_keytab.end(), xkey, CompareNodeKey<key_type, key_compare>(m_comp)
#define TERARK_PZ_FUN(func)   std::func
#include "sorted_table_mf_1.hpp"
///////////////////////////////////////////////////////////////////////////////////////////////

// 	size_t key_count() const
// 	{
// 		typename key_tab_t::const_iterator iter = m_keytab.begin();
// 		size_t sz = 0;
// 		while (iter != m_keytab.end() - 1)
// 		{
// 			iter = upper_bound_near(iter, m_keytab.end()-1, *iter,
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
			m_keytab.push_back(node.key());
			m_data.push_back(node.val());
		}
	}
	template<class Output>
	void save_data(Output& output) const
	{
		for (typename key_tab_t::const_iterator iter = m_keytab.begin(); iter != m_keytab.end(); ++iter)
		{
			output << *iter;
		}
	}

	template<class Input>
	void load(Input& input, uint32_t version)
	{
		dump_header header;
		input >> header;
		m_keytab.reserve(header.valCount);
		load_data(m_keytab);
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
		m_keytab.resize(header.valCount);
		m_data.resize(header.valCount);
		m_key_pool.init(header.valCount * sizeof(KeyT));
		if (header.valCount)
		{
			input.ensureRead(&*m_keytab.begin(), sizeof(KeyT)  *header.valCount);
			input.ensureRead(&*m_data .begin(), sizeof(ValueT)*header.valCount);
		}
	}
	template<class Output>
	void dump_save(Output& output) const
	{
		dump_header header(*this);
		output << header;
		if (header.valCount)
		{
			output.ensureWrite(&*m_keytab.begin(), sizeof(KeyT)  *header.valCount);
			output.ensureWrite(&*m_data .begin(), sizeof(ValueT)*header.valCount);
		}
	}
	DATA_IO_REG_DUMP_LOAD_SAVE(parallel_sorted_table)

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
		{
			input.ensureRead(&*m_keytab.begin() + loading.valCount, count*sizeof(KeyT));
			input.ensureRead(&*m_data .begin() + loading.valCount, count*sizeof(ValueT));
		}
		loading.valCount += count;
	}
	//! 准备 dump_load_merged, 如预分配空间等
	void prepair_dump_load_merged(const dump_header& loaded)
	{
		m_key_pool.init(sizeof(key_type)*loaded.valCount);
		m_keytab.resize(loaded.valCount);
		m_data.resize(loaded.valCount);
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
		{
			output.ensureWrite(&*m_keytab.begin(), sizeof(KeyT)  *count);
			output.ensureWrite(&*m_data .begin(), sizeof(ValueT)*count);
		}
		header.valCount += count;
	}

	DATA_IO_REG_LOAD_SAVE_V(parallel_sorted_table, 1)
};

template<class KeyT, class ValueT, class KeyCompareT, class IndexElement>
struct ExtIndexType<parallel_sorted_table<KeyT, ValueT, KeyCompareT>, IndexElement>
{
	typedef parallel_sorted_table<KeyT, IndexElement, KeyCompareT> key_tab_t;
	typedef parallel_sorted_table<KeyT, ValueT,       KeyCompareT> multi_t;
};


} } // namespace terark::prefix_zip

#endif // __terark_parallel_sorted_table_h__
