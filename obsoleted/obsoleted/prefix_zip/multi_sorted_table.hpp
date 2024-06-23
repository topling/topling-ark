/* vim: set tabstop=4 : */
#ifndef __terark_multi_sorted_table_h__
#define __terark_multi_sorted_table_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

#include "zstring_table.hpp"

namespace terark { namespace prefix_zip {

/// 前置声明
template<class BaseTableT>
class MultiSortedTable;

template<class BaseTableT, class IndexTableT>
class MultiSortedTable_const_iterator;
template<class BaseTableT, class IndexTableT>
class MultiSortedTable_iterator;

/// @name const operations @{
#define TERARK_M_const
#define TERARK_M_iterator iterator
#define TERARK_M_reverse_iterator reverse_iterator
#include "multi_sorted_table_iter_i.hpp"
/// @}

/// @name iterator define @{
#define TERARK_MULTI_SORTED_TABLE_IS_CONST_ITER
#define TERARK_M_const const
#define TERARK_M_iterator const_iterator
#define TERARK_M_reverse_iterator const_reverse_iterator
#include "multi_sorted_table_iter_i.hpp"
#undef TERARK_MULTI_SORTED_TABLE_IS_CONST_ITER
/// @}

/**
 @brief MultiSortedTable 可以有效包含重复元素的 SortedTable

  但是比 SortedTable<value_type, CompareT, FlagLength, IsUniqueKey=false> 的存储效率更高，
  按 key 查询遍历 value 集合时速度也更快，主要原因是字符串解压速度会比较快
 */
template<class BaseTableT>
class MultiSortedTable
{
public:
	/**
	 @brief 用较小的尺寸表示 key 指针，只能指向 key，不能指向同一 key 的多个 value 中的具体某个

	  handle 的尺寸比 iterator 小得多（iterator 在 32 位系统上占 16 个字节，handle 总是占 4 个字节）
	  用于需要存储大量指向 key 的指针时，如统计所有 key 的频率
	 */
	BOOST_STRONG_TYPEDEF(uint32_t, handle_t);

	typedef typename BaseTableT::dump_header dump_header_base;

	struct dump_header : public dump_header_base
	{
		uint32_t valCount;

		template<class SrcTable>
		explicit dump_header(const SrcTable& tab)
			: dump_header_base(tab), valCount(tab.val_count())
		{
			dump_header_base::valCount = tab.key_count();
		}
		explicit dump_header(const MultiSortedTable& tab)
			: dump_header_base(tab.m_index), valCount(tab.val_count())
		{}
		dump_header() : valCount(0)
		{}

		uint32_t key_count() const { return dump_header_base::valCount; }

		template<class Input> void load(Input& input, uint32_t version)
		{
			dump_header_base::load(input, version);
			input >> valCount;
		}
		template<class Output> void save(Output& output, uint32_t version) const
		{
			dump_header_base::save(output, version);
			output << valCount;
		}
		DATA_IO_REG_LOAD_SAVE_V(dump_header, 1)
	};
	friend struct dump_header;

	/**
	 @brief 从 key 到一个 uint32_t 的映射表

	 这个 uint32_t 存储的是这个 key 的第一个 value 在 m_values 中的下标，
	 这个 key 对应的 value 数目由下一个下标和当前下标的差计算出来，
	 因为 m_index.end() 实际上指向一个末尾的哑元，所以使用 *m_index.end() 存储
	 m_values 的尺寸，这样对 m_index 的处理就很方便，不必再单独为末尾的元素做不同的处理。
	 （ SortedTable 本身末尾就有一个哑元，这里刚好利用了它）。
	 */
	typedef typename ExtIndexType<BaseTableT, uint32_t>::index_t index_t;

	typedef typename BaseTableT::compare_t  compare_t;
	typedef typename BaseTableT::raw_key_t	raw_key_t;
	typedef typename BaseTableT::key_type   key_type;
	typedef typename BaseTableT::value_type value_type;
	typedef typename BaseTableT::size_type  size_type;

	typedef MultiSortedTable_iterator      <BaseTableT, index_t>       iterator;
	typedef MultiSortedTable_const_iterator<BaseTableT, index_t> const_iterator;

	friend class MultiSortedTable_iterator      <BaseTableT, index_t>;
	friend class MultiSortedTable_const_iterator<BaseTableT, index_t>;

	typedef boost::reverse_iterator<iterator>		reverse_iterator;
	typedef boost::reverse_iterator<const_iterator> const_reverse_iterator;

	typedef typename std::vector<value_type>::iterator				 val_iter_t;
	typedef typename std::vector<value_type>::const_iterator		 c_val_iter_t;
	typedef typename std::vector<value_type>::reverse_iterator		 rev_val_iter_t;
	typedef typename std::vector<value_type>::const_reverse_iterator c_rev_val_iter_t;

	typedef typename iterator::difference_type difference_type;

	typedef std::pair<iterator, iterator> range_t;
	typedef std::pair<const_iterator, const_iterator> const_range_t;

private:
	index_t				m_index;
	std::vector<value_type> m_values;

public:
	explicit MultiSortedTable(uint32_t maxPoolSize = TERARK_IF_DEBUG(8*1024, 2*1024*1024),
							  compare_t comp = compare_t())
		: m_index(maxPoolSize, comp)
	{
		*m_index.end() = 0;
	}

	void set_unzip_prefer(unzip_prefer uzp)
	{
		// do nothing...
		// m_multiple.set_unzip_prefer(uzp);
	}
//	compare_t& comp() { return m_index.comp(); }
	compare_t comp() const { return m_index.comp(); }

//////////////////////////////////////////////////////////////////////////
	void swap(MultiSortedTable& that)
	{
		m_index.swap(that.m_index);
		m_values.swap(that.m_values);
	}
	friend void swap(MultiSortedTable& x, MultiSortedTable& y) { x.swap(y); }

	void clear()
	{
		m_index.clear();
		m_values.clear();
	}

	void reserve(size_type val_size, size_type key_size = 0)
	{
		if (0 == key_size)
			key_size = size_type(val_size * 0.2);
		m_index.reserve(key_size + 1);
		m_values.reserve(val_size);
	}

	/**
	 @{
	 @brief 为 iter 指向的 key 关联的 values 排序

	 @return 返回排序的 value_count
	 @note iter 必须指向 key-values 中的第一个 value
	 */
	template<class CompValueT>
	size_t sort_values(iterator iter, CompValueT compVal)
	{
		typename index_t::iterator next = iter.m_iter; ++next;
		std::sort(m_values.begin() + *iter.m_iter,
				  m_values.begin() + *next, compVal
				  );
		return *next - *iter.m_iter;
	}
	template<class CompValueT>
	size_t sort_values(handle_t handle, CompValueT compVal)
	{
		typename index_t::iterator ibeg = begin().m_iter;
		typename index_t::iterator curr = ibeg + handle.t;
		typename index_t::iterator next = curr; ++next;
		std::sort(m_values.begin() + *curr,
				  m_values.begin() + *next, compVal
				  );
		return *next - *curr;
	}
	//@}

	//! sort all values associated with their key
	//!
	//! @return same as distinct_size
	template<class CompValueT>
	size_t sort_values(CompValueT compVal, bool printLog = true)
	{
		typename index_t::const_iterator iter = m_index.begin();
	//	uint32_t nthKey = 0;
		while (iter != m_index.end())
		{
			typename index_t::const_iterator next = iter; ++next;
		//	if (printLog && (nthKey % TERARK_IF_DEBUG(5000, 100000) == 0 || uint32_t(*next - *iter) > 10000))
		//	{
		//		printf("nthKey=%d, val_index(%lu, %lu)\n", nthKey, *iter, *next);
		//	}
			std::sort(m_values.begin() + *iter, m_values.begin() + *next, compVal);
			iter = next;
	//		++nthKey;
		}
	//	assert(m_index.size() == nthKey);
		assert(m_values.size() == *m_index.end());
		return m_values.size();
	}

	size_type key_count() const	{ return m_index.size(); }
	size_type val_count() const	{ return m_values.size(); }

//	size_type distinct_size() const	{ return key_count(); }

	bool empty() const { return m_values.empty(); }
	size_type size() const	{ return val_count(); }

////////////////////////////////////////////////////////////////////////

	const key_type& lastKey() const { return m_index.lastKey(); }

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
	void raise_error(const key_type& xkey, const char* func, const char* file, int line)
	{
		string_appender<> oss;
		oss << file << ":" << line << ", key='" << xkey;
		if (empty())
			oss << "', is empty";
		else
			oss << "', lastKey='" << lastKey();

		oss	<< "', key must not less than lastKey.\n"
			<< "in function: " << func << "\n"
			<< "other member[\n";
		writeInfo(oss, "   total", *this) << "\n";
		writeInfo(oss, "   index", m_index) << "\n";
		oss << "   val_array.size=" << m_values.size() << ",\n"
			<< "]";
		fprintf(stderr, "%s\n", oss.str().c_str());
		throw std::logic_error(oss.str().c_str());
	}
public:
	/**
	 @{
	 @brief push_back key-value(s)
	 */
	void push_back(const key_type& key, const value_type& val)
	{
		int icomp = empty() ? -1 : EffectiveCompare(comp(), lastKey(), key);
		if (icomp > 0)
			raise_error(key, BOOST_CURRENT_FUNCTION, __FILE__, __LINE__);
		else if (icomp < 0)
			m_index.push_back(key, m_values.size());

		m_values.push_back(val);
		*m_index.end() = m_values.size(); // set last dummy
	}

	template<class IterT>
	void push_back(const key_type& key, const IterT& first, const IterT& last)
	{
		if (first == last) return;
		int icomp = empty() ? -1 : EffectiveCompare(comp(), lastKey(), key);
		if (icomp > 0)
		{
			raise_error(key, BOOST_CURRENT_FUNCTION, __FILE__, __LINE__);
		}
		else if (icomp < 0)
		{
			m_index.push_back(key, m_values.size());
		}
		m_values.insert(m_values.end(), first, last);

		*m_index.end() = m_values.size(); // set last dummy
	}

	/**
	 @brief make sure std::distance(first, last) == count
	 */
	template<class IterT>
	void push_back(const key_type& key, const IterT& first, const IterT& last, uint32_t count)
	{
		if (0 == count)
		{
			assert(first == last);
			return;
		}
		size_t old_val_count = this->val_count();

		push_back(key, first, last);

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

	key_type key(handle_t handle) const
	{
		return m_index.key(typename index_t::handle_t(handle.t));
	}

	bool is_zipped(const_iterator iter) const
	{
		return m_index.is_zipped(iter.m_iter);
	}

	size_t unzip_key_size() const { return m_index.unzip_key_size(); }

	size_t used_pool_size()  const { return m_index.used_pool_size(); }
	size_t total_pool_size() const { return m_index.total_pool_size(); }
	size_t available_pool_size() const { return m_index.available_pool_size(); }

	size_t used_mem_size() const
	{
		return m_index.used_mem_size()  + sizeof(value_type) * m_values.size();
	}
	size_t total_mem_size() const
	{
		return m_index.total_mem_size() + sizeof(value_type) * m_values.capacity();
	}

	/**
	 @brief 释放浪费的空间

	  如果已用的空间小于 80\%，就释放掉多于的内存
	  一般用于冻结一个 MultiSortedTable 调用该函数之后，不要再加入新元素
	 */
	void trim_extra()
	{
		if ((double)m_values.size()/m_values.capacity() < 0.8)
		{
			// actually trim extra element...
			std::vector<value_type> temp(m_values);
			m_values.swap(temp);
		}
		m_index.trim_extra();
	}

	template<class ValueCompare>
	value_type max_value(ValueCompare vcomp) const
	{
		TERARK_RT_assert(!empty(), std::invalid_argument);

		// 会多比较一次，循环中第一次是自己和自己比较
		value_type val = m_values[m_index[1]-1];
		typename index_t::const_iterator iter = m_index.begin() + 1;
		for (; iter != m_index.end()-1; ++iter)
		{
			value_type val2 = m_values[*iter - 1];
			if (vcomp(val, val2))
				val = val2;
		}
		return val;
	}

public:
//////////////////////////////////////////////////////////////////////////
/// @name non-const operations @{
#define TERARK_M_const const
#define TERARK_M_iterator const_iterator
#define TERARK_M_reverse_iterator const_reverse_iterator
#include "multi_sorted_table_mf.hpp"
/// @}

/// @name const operations @{
#define TERARK_M_const
#define TERARK_M_iterator iterator
#define TERARK_M_reverse_iterator reverse_iterator
#include "multi_sorted_table_mf.hpp"
/// @}
//////////////////////////////////////////////////////////////////////////
	template<class ResultContainer, class CompatibleCompare>
	void query_result(const key_type& key, ResultContainer& result, CompatibleCompare comp) const
	{
		copy_range(equal_range(key, comp), result);
	}

// 	template<class CompatibleCompare>
// 	std::vector<value_type> query_result(const key_type& key, const CompatibleCompare comp) const
// 	{
// 		std::vector<value_type> result;
// 		copy_range(equal_range(key, comp), result);
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
	void query_result(const key_type& key, ResultContainer& result) const
	{
		typename index_t::const_range_t range = m_index.equal_range(key);
		std::copy(m_values.begin() + *range.first, m_values.begin() + *range.second, std::back_inserter(result));
	}

	std::vector<value_type> query_result(const key_type& key) const
	{
		std::vector<value_type> result;
		query_result(key, result);
		return result;
	}

	template<class ResultContainer>
	void query_result(const const_iterator& iter, ResultContainer& result) const
	{
		assert(this->end() != iter);

		result.insert(result.end(),
			m_values.begin() + iter.m_iter[0],
			m_values.begin() + iter.m_iter[1]);
	}

	void init(uint32_t maxPoolSize)
	{
		m_index.init(maxPoolSize);
	}

	static void check_key(const key_type& k) { index_t::check_key(k); }

	template<class DataInput>
	void dump_load_segment(DataInput& input, dump_header& loading, const dump_header& loaded)
	{
		uint32_t valCount;
		input >> valCount;

		if (valCount)
			input.ensureRead(&*m_values.begin() + loading.valCount, valCount*sizeof(value_type));

		ptrdiff_t prevIndex = loading.key_count();
		m_index.dump_load_segment(input, loading, loaded);

	//	printf("keyCount=%d, valCount=%d, loading.valCount=%d\n",
	//		loading.key_count(), valCount, loading.valCount);

		for (ptrdiff_t i = prevIndex; i != loading.key_count(); ++i)
		{
			m_index[i] += loading.valCount;
		}
		loading.valCount += valCount;
	}
	void prepair_dump_load_merged(const dump_header& loaded)
	{
		m_values.resize(loaded.valCount); // maybe large than actual size...
		m_index.prepair_dump_load_merged(loaded);
	}
	template<class DataInput>
	void complete_dump_load_merged(DataInput& input, dump_header& loading, const dump_header& loaded)
	{
		if (loaded.valCount != loading.valCount)
		{
			string_appender<> oss;
			oss << "data format error, val_count[saved=" << loaded.valCount
				<< ", actual=" << loading.valCount << "]";
			throw DataFormatException(oss.str());
		}
		m_index.complete_dump_load_merged(input, loading, loaded);

	//	m_values.resize(loading.valCount);
		trim_extra();

		*m_index.end() = loading.valCount;
	}

	template<class DataOutput>
	void dump_save_segment(DataOutput& output, dump_header& header) const
	{
		uint32_t valCount = uint32_t(m_values.size());
		output << valCount;
		if (valCount)
			output.ensureWrite(&*m_values.begin(), valCount*sizeof(value_type));
		m_index.dump_save_segment(output, header);
		header.valCount += valCount;
	}

	template<class DataInput> void dump_load(DataInput& input)
	{
		this->clear();

		dump_header header;
		input >> header;
		header.check();

		if (header.valCount)
		{
			m_values.resize(header.valCount);
			input.ensureRead(&*m_values.begin(), header.valCount*sizeof(value_type));
		}
		m_index.dump_load(input);
		*m_index.end() = m_values.size();
	}
	template<class DataOutput> void dump_save(DataOutput& output) const
	{
		dump_header header(*this);
		header.valCount = m_values.size(); // must modify this val
		output << header;

		if (header.valCount)
		{
			output.ensureWrite(&*m_values.begin(), m_values.size()*sizeof(value_type));
		}
		m_index.dump_save(output);
	}
	DATA_IO_REG_DUMP_LOAD_SAVE(MultiSortedTable)

protected:
	template<class DataInput > void load(DataInput& input, unsigned int version)
	{
		dump_header header;
		input >> header;
		input >> m_values;
		input >> m_index;
		*m_index.end() = m_values.size();
	}
	template<class DataOutput> void save(DataOutput& output, unsigned int version) const
	{
		dump_header header(*this);
		output << header;
		output << m_values;
		output << m_index;
	}
	DATA_IO_REG_LOAD_SAVE_V(MultiSortedTable, 1)
};

} } // namespace terark::prefix_zip


#endif // __terark_multi_sorted_table_h__

