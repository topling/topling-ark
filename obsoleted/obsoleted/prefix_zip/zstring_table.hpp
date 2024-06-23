/* vim: set tabstop=4 : */
/**
 @note 注意事项

  如果目标平台的 std::string 实现有问题，需要定义宏：TERARK_PREFIXZIP_SAFE_UNZIP
  目前，不需要定义宏 TERARK_PREFIXZIP_SAFE_UNZIP 在 windows+msvc 和 linux+gcc 下均运行良好。

  - 如果定义该宏，ZStringTable::key() 将比较慢，但是对任何符合
    C++ 标准的 string ，都会产生正确的代码。

  - 如果未定义该宏，则不能保证对任何符合 C++ 标准的 string 都产生正确的代码。
 */

#ifndef __terark_zstring_table_h__
#define __terark_zstring_table_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
//# pragma warning(disable: 4267)
#endif

#define TERARK_PREFIXZIP_SAFE_UNZIP

#include <stdexcept>

#include "unzip_prefer.hpp"
#include "ext_index.hpp"
#include "zstring.hpp"
#include "string_pool.hpp"

#include "../DataBuffer.hpp"
#include "../string_compare.hpp"
#include <terark/io/DataIO.hpp>
#include <terark/io/InputStream.h>

#include <boost/operators.hpp>
//#include <boost/iterator.hpp>
#include <boost/iterator/reverse_iterator.hpp>
#include <boost/static_assert.hpp>

#include "dump_header.hpp"

namespace terark { namespace prefix_zip {

template<uint_t FlagLength>
class ZippedStringPool : public AutoGrownMemIO
{
	uint32_t m_key_count;
public:
	typedef ZString<FlagLength> zstr_t;
	BOOST_STATIC_CONSTANT(uint_t, MAX_ZSTRING = zstr_t::MAX_ZSTRING);
	BOOST_STATIC_ASSERT(MAX_ZSTRING < 64 * 1024);

	explicit ZippedStringPool(uint32_t maxSize)	: AutoGrownMemIO(maxSize), m_key_count(0) { }

	uint32_t key_count() const { return m_key_count; }

	uint32_t total_size() const { return AutoGrownMemIO::size(); }

	void reset() { AutoGrownMemIO::reset(); m_key_count = 0; }

	///@note if the pool is to be full, throw OutOfSpaceException
	uint32_t put(const std::string& str, uint_t nPrefix, uint_t diff)
	{
		assert(nPrefix <= str.size());

		uint_t nRequire;
		if (0 == diff)
			nRequire = str.size();
		else
			nRequire = str.size() - nPrefix + FlagLength;

		uint32_t offset = this->tell();
		if (offset + nRequire > this->size())
		{
			string_appender<> oss;
			oss << BOOST_CURRENT_FUNCTION
				<< ": [str='" << str << "', nPrefix=" << nPrefix << ", diff=" << diff << "]\n"
				<< "other vars: [offset=" << offset
				<< ", nRequire=" << nRequire
				<< ", this.size=" << this->size() << "]"
				;
			throw OutOfSpaceException(oss.str().c_str());
		}
		if (0 == diff)
		{
			// 必须是 str.c_str()[0], 不可为 str[0],
			// 因为 str 一旦为空，str[0] 将是未定义的
			const byte firstByte = byte(str.c_str()[0]);

			if (ZString_EscapeByte(firstByte) != -1 || 0xFF == firstByte)
			{
				this->writeByte(0xFF);
			}
			this->write(str.c_str(), str.size());
		} else {
			zstr_t* zipped = zstr(this->tell());
			zipped->setDiff(diff);
			zipped->setPrefixLength(nPrefix);
			memcpy(zipped->suffix(), str.c_str() + nPrefix, str.size() - nPrefix);
			this->seek(offset + nRequire);
		}
		m_key_count++;
		return offset;
	}

	zstr_t* zstr(uint32_t offset) const
	{
		assert(offset < AutoGrownMemIO::size());
		return (zstr_t*)(AutoGrownMemIO::buf() + offset);
	}
};

inline void check_flag_length(uint32_t loadedFlagLength, uint32_t m_flagLength)
{
	if (m_flagLength != loadedFlagLength) {
		string_appender<> oss;
		oss << "FlagLength not match [loaded=" << loadedFlagLength << ", actual=" << m_flagLength << "]";
		throw DataFormatException(oss.str());
	}
}

template<uint_t FlagLength>
struct ZStringTable_dump_header : public Table_dump_header
{
	uint32_t dataSize;
	uint32_t m_flagLength;

	ZStringTable_dump_header()
	{
		this->m_flagLength = FlagLength;
	}
	template<class SrcTable>
	explicit ZStringTable_dump_header(const SrcTable& src)
		: Table_dump_header(src)
	{
		BOOST_STATIC_ASSERT(FlagLength == SrcTable::FLAG_LENGTH);
		m_flagLength = SrcTable::FLAG_LENGTH;
		dataSize = src.used_pool_size();
	}

	bool less_than(const ZStringTable_dump_header& loaded) const
	{
		return valCount < loaded.valCount;
	}

	void check() const { check_flag_length(this->m_flagLength, FlagLength); }

	DATA_IO_REG_LOAD_SAVE_V(ZStringTable_dump_header, 1,
		& vmg.template base<Table_dump_header>(this)
		& m_flagLength
		& dataSize
		)
};

/// 前置声明
template<class ValueT, class CompareT, uint_t FlagLength, bool IsUniqueKey = true>
class ZStringTable;

template<class ValueT, class CompareT, uint_t FlagLength, bool IsUniqueKey,
		 class IndexIter,
		 class BaseIteratorDef,
		 class SelfReflected
>
class ZStringTable_IteratorBase : public BaseIteratorDef
{
	friend class ZStringTable<ValueT, CompareT, FlagLength, IsUniqueKey>;

protected:
	IndexIter m_iter;

	ZStringTable_IteratorBase(IndexIter iter) : m_iter(iter) {}
	ZStringTable_IteratorBase() {}

public:
	typedef typename BaseIteratorDef::reference reference;
	typedef typename BaseIteratorDef::difference_type difference_type;
	typedef IndexIter index_iterator;

	uint32_t get_offset() const { return (*m_iter).offset; }

	bool equal_next() const { return (*m_iter).offset == (*(m_iter+1)).offset; }
	bool equal_prev() const	{ return (*m_iter).offset == (*(m_iter-1)).offset; }

	reference operator*() const { return (*m_iter).val(); }

	SelfReflected& operator++() { ++m_iter; return static_cast<SelfReflected&>(*this); }
	SelfReflected& operator--() { --m_iter; return static_cast<SelfReflected&>(*this); }

	friend bool operator==(const SelfReflected& x, const SelfReflected& y)
	{
		return x.m_iter == y.m_iter;
	}

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

	friend difference_type operator-(const SelfReflected& x, const SelfReflected& y)
	{
		return x.m_iter - y.m_iter;
	}
	friend bool operator<(const SelfReflected& x, const SelfReflected& y)
	{
		return x.m_iter < y.m_iter;
	}
};
#define TERARK_ZSTRING_TABLE_ITERATOR_BASE(TableClass, IterClass, ConstVal, IterType) \
	ZStringTable_IteratorBase<	\
		ValueT, CompareT, FlagLength, IsUniqueKey, \
		typename ZStringTable<ValueT, CompareT, FlagLength, IsUniqueKey>::index_t::IterType,\
		boost::random_access_iterator_helper< \
			IterClass<ValueT, CompareT, FlagLength, IsUniqueKey>,\
			ConstVal,	\
			ptrdiff_t,	\
			ConstVal*,	\
			ConstVal&	\
		>,				\
		IterClass<ValueT, CompareT, FlagLength, IsUniqueKey> >
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

template<class ValueT, class CompareT, uint_t FlagLength, bool IsUniqueKey>
class ZStringTable_ConstIterator;

template<class ValueT, class CompareT, uint_t FlagLength, bool IsUniqueKey>
class ZStringTable_Iterator :
	public  TERARK_ZSTRING_TABLE_ITERATOR_BASE(ZStringTable, ZStringTable_Iterator, ValueT, iterator)
{
	typedef TERARK_ZSTRING_TABLE_ITERATOR_BASE(ZStringTable, ZStringTable_Iterator, ValueT, iterator) super;
	friend class ZStringTable<ValueT, CompareT, FlagLength, IsUniqueKey>;
	friend class ZStringTable_ConstIterator<ValueT, CompareT, FlagLength, IsUniqueKey>;

	ZStringTable_Iterator(typename super::index_iterator iter) : super(iter) {}

public:
	ZStringTable_Iterator() {}
};

template<class ValueT, class CompareT, uint_t FlagLength, bool IsUniqueKey>
class ZStringTable_ConstIterator :
	public  TERARK_ZSTRING_TABLE_ITERATOR_BASE(const ZStringTable, ZStringTable_ConstIterator, const ValueT, const_iterator)
{
	typedef TERARK_ZSTRING_TABLE_ITERATOR_BASE(const ZStringTable, ZStringTable_ConstIterator, const ValueT, const_iterator) super;
	friend class ZStringTable<ValueT, CompareT, FlagLength, IsUniqueKey>;

	ZStringTable_ConstIterator(typename super::index_iterator iter) : super(iter) {}
public:
	ZStringTable_ConstIterator() {}
	ZStringTable_ConstIterator(ZStringTable_Iterator<ValueT, CompareT, FlagLength, IsUniqueKey> iter)
		: super(iter.m_iter) { }
};

/**
 @ingroup prefix_zip
 @brief 压缩的字符串映射表

  只能按 CompareT 所示的顺序将元素加入该映射表，一般不单独使用
  主要用于 StringTable 的固定索引
 */
template<class ValueT, class CompareT, uint_t FlagLength, bool IsUniqueKey>
class ZStringTable
{
	typedef typename IndexNode<ValueT>::type node_t;

public:
	BOOST_STATIC_CONSTANT(uint_t, FLAG_LENGTH = FlagLength);

	typedef std::vector<node_t>			 index_t;
	typedef ZString<FlagLength>			 zstr_t;
	typedef ZippedStringPool<FlagLength> zpool_t;
	typedef ZStringTable_dump_header<FlagLength>  dump_header;

	/**
	 @brief 仅保存 string 部分，无对应的 value

	 用于将 table 中的 string 导出
	 */
	typedef ZStringTable<nil, CompareT, FlagLength, true> zstr_set;
//	friend  class ZStringTable<nil, CompareT, FlagLength, true>; //!< for access from zstr_set

	typedef std::string raw_key_t;
	typedef std::string key_type;
	typedef CompareT compare_t;
	typedef CompareT key_compare;
	typedef ValueT   value_type;
	typedef typename index_t::size_type size_type;

	typedef ZStringTable_Iterator<ValueT, CompareT, FlagLength, IsUniqueKey>		iterator;
	typedef ZStringTable_ConstIterator<ValueT, CompareT, FlagLength, IsUniqueKey> const_iterator;

	typedef boost::reverse_iterator<iterator>		reverse_iterator;
	typedef boost::reverse_iterator<const_iterator> const_reverse_iterator;

	typedef typename iterator::difference_type difference_type;

	typedef std::pair<iterator, iterator> range_t;
	typedef std::pair<const_iterator, const_iterator> const_range_t;

	BOOST_STATIC_CONSTANT(uint_t, MAX_ZSTRING = zstr_t::MAX_ZSTRING);

	typedef boost::integral_constant<bool, IsUniqueKey> is_unique_key_t;

private:
	index_t  m_index;
	zpool_t  m_pool;
	CompareT m_comp;
	key_type m_lastKey;
	typedef void (ZStringTable::*unzip_aux_ptr_t)(const_iterator iter, key_type& str) const;
	typename boost::mpl::if_c<IsUniqueKey, nil, unzip_aux_ptr_t>::type mf_unzip_aux;

	void do_unzip_impl(const_iterator iter, char* pszBuffer, uint_t nPrefix) const;

	void do_unzip(const_iterator iter, key_type& str, uint_t nPrefix) const;
	zstr_t* zstr(const_iterator iter) const
	{
		return m_pool.zstr((*iter.m_iter).offset);
	}

	void push_back_impl(const key_type& xkey, const ValueT& val, uint_t nPreifx, uint_t diff);

public:
	explicit ZStringTable(uint32_t maxPoolSize = TERARK_IF_DEBUG(8*1024, 2*1024*1024),
						  CompareT comp = CompareT())
		: m_pool(maxPoolSize), m_comp(comp)
	{
		// push_back first dummy value..
		m_index.push_back(node_t(0));
		set_unzip_prefer(uzp_middle_dup);
	}
	ZStringTable(const ZStringTable& rhs)
		: m_index(rhs.m_index), m_pool(0), m_lastKey(rhs.m_lastKey)
	{
		m_pool.clone(rhs.m_pool);
		mf_unzip_aux = rhs.mf_unzip_aux;
	}
	ZStringTable& operator=(const ZStringTable& rhs)
	{
	// not exception safe, but more efficient
		if (this != &rhs)
		{
			m_index = rhs.m_index;
			m_pool.clone(rhs.m_pool);
			m_lastKey = rhs.m_lastKey;
			mf_unzip_aux = rhs.mf_unzip_aux;
		}

	//	ZStringTable(rhs).swap(*this); // exception safe, but low efficient

		return *this;
	}

#define TERARK_PZ_SEARCH_FUN_ARG_LIST_II  first, last, xkey, comp, typename proper_bool<CompatibleCompare>::type()
#define TERARK_PZ_SEARCH_FUN_ARG_LIST_BE_1  this->begin(), this->end(), xkey, comp, typename proper_bool<CompatibleCompare>::type()
#define TERARK_PZ_SEARCH_FUN_ARG_LIST_BE_0  this->begin(), this->end(), xkey, m_comp, is_unique_key_t()
#define TERARK_PZ_FUN(func)   func##_impl
#define TERARK_IS_IN_ZSTRING_TABLE
#include "sorted_table_mf_1.hpp"
#undef  TERARK_IS_IN_ZSTRING_TABLE

//////////////////////////////////////////////////////////////////////////
/// @name non-const operations @{
#define TERARK_M_const
#define TERARK_M_iterator iterator
#define TERARK_M_reverse_iterator reverse_iterator
#include "zstring_table_mf.hpp"
/// @}

/// @name const operations @{
#define TERARK_M_const const
#define TERARK_M_iterator const_iterator
#define TERARK_M_reverse_iterator const_reverse_iterator
#include "zstring_table_mf.hpp"
/// @}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
	void swap(ZStringTable& that)
	{
		m_index.swap(that.m_index);
		m_pool.swap(that.m_pool);
		m_lastKey.swap(that.m_lastKey);
		std::swap(mf_unzip_aux, that.mf_unzip_aux);
	}
	friend void swap(ZStringTable& x, ZStringTable& y) { x.swap(y); }

	void clear()
	{
	//	m_index.clear();
		index_t().swap(m_index); // actual clear
		m_index.push_back(node_t(0));
		m_pool.seek(0);
		m_lastKey.clear();
	}

	const key_type& lastKey() const { return m_lastKey; }

//	size_type key_count()     const	{ return key_count_aux(is_unique_key_t()); }
	size_type key_count()     const	{ return m_pool.key_count(); }
//	size_type distinct_size() const	{ return key_count(); }

	/**
	 @{
	 @brief push_back key-value(s)
	 */
	void push_back(const key_type& xkey, const ValueT& val);

	template<class IterT>
	void push_back(const key_type& xkey, const IterT& first, const IterT& last)
	{
		if (first == last) return;

		uint32_t offset = m_pool.tell();
		push_back(xkey, *first);
		IterT iter = first;
		m_index.pop_back(); // pop last dummy
		for (++iter; iter != last; ++iter)
			m_index.push_back(node_t(offset, *iter));
		m_index.push_back(node_t(m_pool.tell())); // put last dummy
	}
	//! 必须确保 std::distance(first, last) == count
	template<class IterT>
	void push_back(const key_type& xkey, const IterT& first, const IterT& last, size_type count)
	{
		size_type old_size = this->size();
		push_back(xkey, first, last);
		TERARK_RT_assert(old_size + count == this->size(), std::invalid_argument);
	}
	//@}

	//! remove last key-value
	void pop_back()
	{
		assert(!empty());
		m_index.pop_back();
		m_pool.seek(m_index.back().offset);
		key(end() - 1, m_lastKey); // restore m_lastKey
	}

	uint_t strlen(const_iterator iter0, const_iterator iter1) const;

	void set_unzip_prefer(unzip_prefer uzp) { set_unzip_prefer_aux(uzp, is_unique_key_t()); }

	void key(const_iterator iter, key_type& k) const
	{
		assert(end() != iter);
		unzip_aux(iter, k, is_unique_key_t());
	}
	key_type key(const_iterator iter) const
	{
		assert(end() != iter);
		key_type str;
		unzip_aux(iter, str, is_unique_key_t());
		return str;
	}

	key_type key(const_reverse_iterator riter) const
	{
		const_iterator iter = riter.base();
		return key(--iter);
	}
	key_type raw_key(const_iterator iter) const { return key(iter); }

	bool is_zipped(const_iterator iter) const
	{
		if (iter != this->begin() && iter.equal_prev())
			return true;

		zstr_t* zipped = zstr(iter);
		return zipped->getDiff() != 0;
	}
	bool last_is_zipped() const { return is_zipped(this->end() - 1); }

	uint_t primary_prefix_diff(const_iterator iter) const;

	size_t unzip_key_size() const { return unzip_str_size_aux(is_unique_key_t()); }

	size_t  used_pool_size() const { return m_pool.tell(); }
	size_t total_pool_size() const { return m_pool.total_size(); }
	size_t available_pool_size() const { return m_pool.remain(); }

	size_t  used_mem_size() const { return  used_pool_size() + m_index.size()     * sizeof(node_t); }
	size_t total_mem_size() const { return total_pool_size() + m_index.capacity() * sizeof(node_t); }

	void init(uint32_t maxPoolSize)
	{
		clear();
		m_pool.init(maxPoolSize);
	}

	/**
	 @brief 释放浪费的空间

	  如果可用的空间大于 5\%，就新申请一块内存存放字符串池，并将原先的内存释放\n
	  一般用于冻结一个 ZStringTable 时，调用该函数之后，再加入新元素很可能失败
	 */
	void trim_extra()
	{
		if ((double)m_pool.tell()/m_pool.total_size() < 0.95)
		{
			m_pool.resize(m_pool.tell());
		}
		if ((double)m_index.size() / m_index.capacity() < 0.8)
		{
			index_t itemp(m_index);
			itemp.swap(m_index);
		}
	}

	//! low level method
	uint_t get_record_length(const_iterator iter0, const_iterator iter1) const
	{
		assert(iter1 > iter0);
	//	assert((*iter1.m_iter).offset > (*iter0.m_iter).offset); // when save, this maybe raised
		return (*iter1.m_iter).offset - (*iter0.m_iter).offset;
	}

	void pop_back_n(size_type n)
	{
		assert(size() >= n);
		m_index.erase(m_index.end() - n, m_index.end());
		m_pool.seek(m_index.back().offset);
		if (!this->empty())
			synchronize_last_key();
	}

	/**
	 @brief 检查 k 是否过大

	  如果太大，就抛出 std::invalid_argument 异常
	 */
	static void check_key(const key_type& k)
	{
		if (k.size() > MAX_ZSTRING)
		{
			char szMsg[1024];
			sprintf(szMsg, "string size=%d too large for ZStringTable<FlagLength=%d>::MAX_ZSTRING=%d",
				k.size(), FlagLength, MAX_ZSTRING);
			throw std::invalid_argument(szMsg);
		}
	}

//////////////////////////////////////////////////////////////////////////
private:
	void unzip_aux(const_iterator iter, key_type& str, boost:: true_type isUniqueKey) const;
	void unzip_aux(const_iterator iter, key_type& str, boost::mpl::false_ isUniqueKey) const
	{
		(this->*mf_unzip_aux)(iter, str);
	}
	void unzip_aux_lower_upper_bound_std(const_iterator iter, key_type& str) const;
	void unzip_aux_lower_upper_bound_near(const_iterator iter, key_type& str) const;
	void unzip_aux_linear_search(const_iterator iter, key_type& str) const;

	void set_unzip_prefer_aux(enum unzip_prefer prefer, boost::mpl::false_ isUniqueKey)
	{
		switch (prefer)
		{
		case uzp_large_dup:  mf_unzip_aux = &ZStringTable::unzip_aux_lower_upper_bound_std;  break;
		case uzp_small_dup:  mf_unzip_aux = &ZStringTable::unzip_aux_linear_search;			break;
		case uzp_middle_dup: mf_unzip_aux = &ZStringTable::unzip_aux_lower_upper_bound_near;	break;
		}
	}
	void set_unzip_prefer_aux(enum unzip_prefer prefer, boost::mpl::true_ isUniqueKey) {}

	size_t key_count_aux(boost::mpl::false_ isUniqueKey) const
	{
		typename index_t::const_iterator iter = m_index.begin();
		size_t sz = 0;
		while (iter != m_index.end() - 1)
		{
			iter = upper_bound_near(iter, m_index.end()-1, *iter, typename node_t::CompOffset());
			sz++;
		}
		return sz;
	}
	size_t key_count_aux(boost::mpl::true_ isUniqueKey) const { return this->size(); }

	size_t unzip_str_size_aux(boost:: true_type isUniqueKey) const;
	size_t unzip_str_size_aux(boost::mpl::false_ isUniqueKey) const;

	template<class CompatibleCompare>
	struct proper_bool
	{
		// 当 non-unique-key 时,
		// 对未知的 CompatibleCompare 使用效率较低的版本，
		// 对自己的 CompareT 使用效率高的版本
		typedef typename boost::mpl::if_<
			boost::is_same<typename boost::remove_const<CompatibleCompare>::type, CompareT>,
			is_unique_key_t,
			boost::mpl::true_
		>::type type;
	};

	struct IsOffsetEqual
	{
		bool operator()(const node_t& x, const node_t& y) const
		{
			return x.offset == y.offset;
		}
	};

public:
	void synchronize_last_key()
	{
		if (m_index.size() > 1) // not empty
			key(end()-1, m_lastKey);
	}

	void put_last_dummy()
	{
		m_index.push_back(node_t(m_pool.tell())); // last dummy...
	}

	//! 将 key 相同的，只保留第一个 (key, val)
	void unique()
	{
		typename index_t::iterator newEnd =
			std::unique(m_index.begin(), m_index.end(), IsOffsetEqual());
		m_index.resize(newEnd - m_index.begin());
	}

	const index_t& getIndex() const { return m_index; }
	const zpool_t& getZpool() const { return m_pool;  }

	template<class ValueT_2>
	void copy_zstr_pool(const ZStringTable<ValueT_2, CompareT, FlagLength, IsUniqueKey>& y)
	{
		typedef ZStringTable<ValueT_2, CompareT, FlagLength, IsUniqueKey> y_t;
		assert(empty());
		m_pool.clone(y.getZpool());
		m_index.clear();
		m_index.reserve(y.key_count() + 1);
		for (typename y_t::index_t::const_iterator i = y.getIndex().begin(); i != y.getIndex().end(); ++i)
		{
			m_index.push_back(node_t(i->offset));
		}
		synchronize_last_key();
		m_comp = y.comp();
	}

	/**
	 @brief must call on zstr_set
	 */
	template<class ValueT_2, bool IsUniqueKey_2>
	void import_zstr_set(const ZStringTable<ValueT_2, CompareT, FlagLength, IsUniqueKey_2>& zst)
	{
		typedef ZStringTable<ValueT_2, CompareT, FlagLength, IsUniqueKey_2> zst_t;

		//! this type must be zstr_set
		BOOST_STATIC_ASSERT(boost::is_empty<ValueT>::value);

		m_pool.clone(zst.m_pool);
		for (typename zst_t::index_t::const_iterator i = zst.m_index.begin(); i != zst.m_index.end(); )
		{
			m_index.push_back(node_t(i->offset));
			i = upper_bound_near(i, zst.m_index.end(), *i, typename node_t::CompOffset());
		}
		synchronize_last_key();
		m_comp = zst.m_comp;
	}

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
		uint32_t valCount, dataSize;
		input >> valCount >> dataSize;

		assert(valCount && dataSize || !valCount && !dataSize);
		if (valCount)
		{
			input.ensureRead(m_pool.buf() + loading.dataSize, dataSize);
			input.ensureRead(&*m_index.begin() + loading.valCount, valCount*sizeof(node_t));
		}
		for (uint32_t i = loading.valCount; i != loading.valCount + valCount; ++i)
		{
			m_index[i].offset += loading.dataSize;
		}
		loading.valCount += valCount;
		loading.dataSize += dataSize;
	}
	//! 准备 dump_load_merged, 如预分配空间等
	void prepair_dump_load_merged(const dump_header& loaded)
	{
		m_pool.init(loaded.dataSize);
		m_index.resize(loaded.valCount + 1);
	}
	//! 完成 dump_load_merged, 进行收尾工作
	template<class Input>
	void complete_dump_load_merged(Input& input, dump_header& loading, const dump_header& loaded)
	{
		if (this->size() != loading.valCount)
		{
			string_appender<> oss;
			oss << "data format error, valCount[saved=" << this->size() << ", actual=" << loading.valCount << "]";
			throw DataFormatException(oss.str());
		}
		m_pool.seek(loading.dataSize);
		m_index.back().offset = loading.dataSize;
		synchronize_last_key();
		trim_extra();
	}
	//@}

	/**
	 @brief 仅仅是声明存储 merged 的标准方式，这个函数在实际中不会使用

	  ---------- header --------- | .......................... | .......................... | ...
	  total_count total_data_size | count_1 data_size_1 data_1 | count_2 data_size_2 data_2 | ...
	  total_count == count_1 + count_2 + ...
	 */
	template<class Output> void dump_save_merged(Output& output) const
	{
		dump_header header(*this);
		output << header;
		dump_save_segment(output, header);
	}

	/**
	 @brief 将 *this 存储成一个段
	 */
	template<class Output>
	void dump_save_segment(Output& output, dump_header& header, bool includeLastKey=true) const
	{
		uint32_t valCount = this->size();
		uint32_t dataSize = m_pool.tell();

		if (!includeLastKey) {
			valCount = lower_bound_near(m_index.begin(), m_index.end()-2,
				*(m_index.end()-2), typename node_t::CompOffset()) - m_index.begin();
			dataSize = (*(m_index.end()-2)).offset;
			assert(valCount < this->size());
			assert(dataSize < m_pool.tell());
		}
		output << valCount << dataSize;

		assert(valCount && dataSize || !valCount && !dataSize);
		if (dataSize)
		{
			output.ensureWrite(m_pool.buf(), dataSize);
			output.ensureWrite(&*m_index.begin(), valCount*sizeof(node_t));
		}
		header.valCount += valCount;
		header.dataSize += dataSize;
	}
	//@}

	/**
	 @{
	 @brief 直接将内容容不做任何修改 dump 到内存

	 速度最快，但格式不兼容，也不能合并
	 */
	template<class Input > void dump_load(Input& input)
	{
		dump_header header;
		uint32_t valCount, dataSize;

		input >> header;
		input >> valCount >> dataSize;
	//	printf("valCount=%d, dataSize=%d\n", header.valCount, header.dataSize);
		header.check();

		m_pool.init(header.dataSize);
		m_index.resize(header.valCount + 1);

		input.ensureRead(m_pool.buf(), header.dataSize);
		input.ensureRead(&*m_index.begin(), header.valCount*sizeof(node_t));

		m_pool.seek(m_pool.total_size());
		m_index.back().offset = m_pool.total_size();

	//	printf("synchronize_last_key... ");
		synchronize_last_key();
	//	printf(", lastKey=%s\n", m_lastKey.c_str());
	}
	template<class Output> void dump_save(Output& output) const
	{
		dump_header header(*this);
		output << header;
		dump_save_segment(output, header, true);
	}
	//@}
	DATA_IO_REG_DUMP_LOAD_SAVE(ZStringTable)

protected:
	/**
	 @{
	 @brief fast load/save, 比默认的 serialization 快，但比 dump 慢

	 @note 当 FlagLength=2,3 时，可以产生兼容的数据文件，当 FlagLength=4 时，不能产生兼容的数据文件
	 */
	template<class Input > void load(Input& input, unsigned version)
	{
		dump_header header;
		input >> header;
		header.check();

		m_pool.init(header.dataSize);
		m_index.resize(header.valCount + 1);

		input.ensureRead(m_pool.buf(), header.dataSize);

		input >> m_index;

		m_pool.seek(m_pool.total_size());
		m_index.back().offset = m_pool.total_size();

		synchronize_last_key();
	}
	template<class Output> void save(Output& output, unsigned version) const
	{
		dump_header header(*this);

		output << header;
		output.ensureWrite(m_pool.buf(), m_pool.tell());

		output << m_index;
	}
	//@}
	DATA_IO_REG_LOAD_SAVE_V(ZStringTable, 1)
};

template<class ValueT, class CompareT, uint_t FlagLength, bool IsUniqueKey, class IndexElement>
struct ExtIndexType<ZStringTable<ValueT, CompareT, FlagLength, IsUniqueKey>, IndexElement>
{
	typedef ZStringTable<IndexElement, CompareT, FlagLength, true >  index_t;
	typedef ZStringTable<ValueT,       CompareT, FlagLength, false>  multi_t;
};

/**
 @brief 获取 iter 到主前缀的偏移

  主前缀就是前缀与前一个 string 不同的前缀
  如: 中华         <--+--主前缀 primary_prefix
      中华人          |
      中华人民        |
      中华人民共      | <---- primary_prefix_diff
      中华人民共和    |
      中华人民共和国  |
      中华人民共产党  |
	  中华大地     <--+-- iter

 */
template<class ValueT, class CompareT, uint_t FlagLength, bool IsUniqueKey>
uint_t ZStringTable<ValueT, CompareT, FlagLength, IsUniqueKey>::primary_prefix_diff(const_iterator iter) const
{
	assert(iter >= begin() && iter < end());

	uint_t diff = 0;
	for (; ; )
	{
		zstr_t* zipped = zstr(iter-difference_type(diff));
		uint_t theDiff = zipped->getDiff(); // 该 zstr_t 没有压缩
		if (0 == theDiff)
			break;
		diff += theDiff;
	}
	return diff;
}

template<class ValueT, class CompareT, uint_t FlagLength, bool IsUniqueKey>
uint_t ZStringTable<ValueT, CompareT, FlagLength, IsUniqueKey>::strlen(const_iterator iter0, const_iterator iter1) const
{
	assert(iter0 < iter1);
	assert(iter0 >= begin() && iter0 <  end());
	assert(iter1 >  begin() && iter1 <= end());

	zstr_t* zipped = zstr(iter0);
	uint_t diff = zipped->getDiff();
	uint_t slen;
	if (0 == diff) {
		if (zipped->isFirstByteEscape()) {
			slen = get_record_length(iter0, iter1) - 1;
			assert(slen <= MAX_ZSTRING);
		} else {
			slen = get_record_length(iter0, iter1);
			assert(slen <= MAX_ZSTRING);
		}
	} else {
		uint_t nSuffix = get_record_length(iter0, iter1) - FlagLength; // suffix length of zstr_t..
		slen = zipped->prefixLength() + nSuffix;
		assert(slen <= MAX_ZSTRING);
	}
	return slen;
}

template<class ValueT, class CompareT, uint_t FlagLength, bool IsUniqueKey>
size_t ZStringTable<ValueT, CompareT, FlagLength, IsUniqueKey>::
unzip_str_size_aux(boost::mpl::true_ isUniqueKey) const
{
	size_t size = 0;
	for (const_iterator iter = begin(); iter != end(); ++iter)
	{
		// 用其它方式，不管怎么优化
		// 总避免不了要用至少一个字节保存 string 长度或者结束的 '\0'
		size += strlen(iter, iter+1) + 1;
	}
	return size;
}

template<class ValueT, class CompareT, uint_t FlagLength, bool IsUniqueKey>
size_t ZStringTable<ValueT, CompareT, FlagLength, IsUniqueKey>::
unzip_str_size_aux(boost::mpl::false_ isUniqueKey) const
{
	size_t size = 0;
	for (typename index_t::const_iterator iter0 = m_index.begin(); iter0 != m_index.end() - 1;)
	{
		typename index_t::const_iterator iter1 = std::upper_bound(
			iter0, m_index.end(), *iter0, typename node_t::CompOffset());

		// 用其它方式，不管怎么优化
		// 总避免不了要用至少一个字节保存 string 长度或者结束的 '\0'
		size += (strlen(iter0, iter1) + 1) * (iter1 - iter0);
		iter0 = iter1;
	}
	return size;
}

/*
 * [0] 中华             getDiff() = 0, text() = 中华
 * [1] 中华人           getDiff() = 1, nPrefix = 4, suffix() = 人
 * [2] 中华人民         getDiff() = 1, nPrefix = 6, suffix() = 民
 * [3] 中华人民共       getDiff() = 1, nPrefix = 8, suffix() = 共
 * [4] 中华人民共和     getDiff() = 1, nPrefix =10, suffix() = 和
 * [5] 中华人民共和国   getDiff() = 1, nPrefix =12, suffix() = 国
 **[6] 中华人民共同     getDiff() = 3, nPrefix =10, suffix() = 同
 * [7] 中华人民共同奋斗 getDiff() = 1, nPrefix =12, suffix() = 奋斗
 */
template<class ValueT, class CompareT, uint_t FlagLength, bool IsUniqueKey>
void ZStringTable<ValueT, CompareT, FlagLength, IsUniqueKey>::
unzip_aux(const_iterator iter, std::string& str, boost::mpl::true_ isUniqueKey) const
{
	uint_t len = this->strlen(iter, iter+1);
	assert(len <= MAX_ZSTRING);

	do_unzip(iter, str, len);
}

// 这样探测，实际上效率并不会提高太多，因为搜索范围是先扩大，后收缩，所以比 lower_bound/upper_bound 要慢一倍
// 但是，因为搜索范围不会扩大很多，可以比较有效地利用 CPU cache
//
// 对 12,090,803 条 (xkey, uint32_value) 进行测试，其中不同的 key 有 1,953,688 个
// 使用 unzip_aux_lower_upper_bound_near 搜索，耗时  18,921 毫秒
// 使用 unzip_aux_lower_upper_bound_std  搜索，耗时  27,609 毫秒
// 使用 unzip_aux_linear_search          搜索，耗时 127,796 毫秒

template<class ValueT, class CompareT, uint_t FlagLength, bool IsUniqueKey>
void ZStringTable<ValueT, CompareT, FlagLength, IsUniqueKey>::
unzip_aux_lower_upper_bound_std(const_iterator iter, std::string& str) const
{
	const_iterator iter0 = std::lower_bound(m_index.begin(), iter.m_iter, *iter.m_iter, typename node_t::CompOffset());
	const_iterator iter1 = std::upper_bound(iter.m_iter+1, m_index.end(), *iter.m_iter, typename node_t::CompOffset());
	uint_t len = this->strlen(iter0, iter1);
	assert(len <= MAX_ZSTRING);

	do_unzip(iter0, str, len);
}

template<class ValueT, class CompareT, uint_t FlagLength, bool IsUniqueKey>
void ZStringTable<ValueT, CompareT, FlagLength, IsUniqueKey>::
unzip_aux_lower_upper_bound_near(const_iterator iter, std::string& str) const
{
	const_iterator iter0 = lower_bound_near(m_index.begin(), iter.m_iter, *iter.m_iter, typename node_t::CompOffset());
	const_iterator iter1 = upper_bound_near(iter.m_iter+1, m_index.end(), *iter.m_iter, typename node_t::CompOffset());
	uint_t len = this->strlen(iter0, iter1);
	assert(len <= MAX_ZSTRING);

	do_unzip(iter0, str, len);
}

template<class ValueT, class CompareT, uint_t FlagLength, bool IsUniqueKey>
void ZStringTable<ValueT, CompareT, FlagLength, IsUniqueKey>::
unzip_aux_linear_search(const_iterator iter, std::string& str) const
{
	const_iterator iter0 = iter, iter1 = iter+1;
	while (m_index.begin() != iter0.m_iter)
	{
		--iter0.m_iter;
		if ((*iter0.m_iter).offset != (*iter.m_iter).offset)
		{
			++iter0.m_iter;
			break;
		}
	}
	while (m_index.end() != iter1.m_iter && (*iter1.m_iter).offset == (*iter.m_iter).offset)
	{
		++iter1.m_iter;
	}
	uint_t len = this->strlen(iter0, iter1);
	assert(len <= MAX_ZSTRING);

	do_unzip(iter0, str, len);
}

template<class ValueT, class CompareT, uint_t FlagLength, bool IsUniqueKey>
void ZStringTable<ValueT, CompareT, FlagLength, IsUniqueKey>::
do_unzip(const_iterator iter, std::string& str, uint_t len) const
{
	if (len > MAX_ZSTRING)
	{
		string_appender<> oss;
		oss << "unzip error, len = " << len << ", MAX_ZSTRING = " << MAX_ZSTRING;
		throw std::length_error(oss.str());
	}
// #ifdef TERARK_PREFIXZIP_SAFE_UNZIP
// 	DataBufferPtr buf(len);
// 	do_unzip_impl(iter, (char*)buf->data(), len);
// 	str.assign((char*)buf->data(), len);
// #else
//
// 	// 必须定义一个 temp string，否则如果直接在 str 上操作，
// 	// 对于采用引用计数+copy on write 实现的 string，改变 str 将会导致程序错误
// 	// 目前已知在 gcc 自带的 string 实现中会出现此错误
// 	// 以下两行是以前导致错误的代码：
// 	// str.resize(len);
// 	// do_unzip_impl(iter, const_cast<char*>(str.data()), len);
//
// 	std::string temp;
// 	temp.resize(len);
// 	do_unzip_impl(iter, const_cast<char*>(temp.data()), len);
// 	str.swap(temp);
// #endif

	// 使用 &*str.begin() 胜过使用 const_cast<char*>(str.data())
	// 前者可以确保 string 的内部表达是 mutable, 后者就不一定
	str.resize(len);
 	do_unzip_impl(iter, &*str.begin(), len);
}

template<class ValueT, class CompareT, uint_t FlagLength, bool IsUniqueKey>
void ZStringTable<ValueT, CompareT, FlagLength, IsUniqueKey>::
do_unzip_impl(const_iterator iter, char* pszBuffer, uint_t nPrefix) const
{
#if defined(_DEBUG) || defined(DEBUG) || !defined(NDEBUG)
	int loop_count = 0;
#endif
	while (true)
	{
#if defined(_DEBUG) || defined(DEBUG) || !defined(NDEBUG)
		loop_count++;
#endif
		assert(iter >= this->begin());
		assert(iter <  this->end());

		zstr_t* zipped = zstr(iter);

		uint_t diff = zipped->getDiff();
		if (0 == diff) {
			memcpy(pszBuffer, zipped->text(), nPrefix);
			break;
		}
		uint_t myPrefix = zipped->prefixLength();
		if (nPrefix > myPrefix)
		{
	#if defined(_DEBUG) || !defined(NDEBUG)
			const_iterator next = next_larger(iter);
			uint_t mySuffixLength = get_record_length(iter, next) - FlagLength;
			assert(nPrefix - myPrefix <= mySuffixLength);
			if (nPrefix - myPrefix <= mySuffixLength)
				;
			else
			{
				fprintf(stderr, "fatal: %s:%d, iter.nth=%d, nPrefix=%d, myPrefix=%d, mySuffixLength=%d, mem=%08X%08X",
					__FILE__, __LINE__, iter-begin(), nPrefix, myPrefix, mySuffixLength, *(long*)zipped, ((long*)zipped)[1]);
				TERARK_IF_DEBUG(assert(0), exit(1));
			}
			memset(pszBuffer, 0, myPrefix);
	#endif
			memcpy(pszBuffer + myPrefix, zipped->suffix(), nPrefix - myPrefix);
			nPrefix = myPrefix;
			iter -= diff;
		} else {
			iter -= diff;
		//	assert(zstr_t::MAX_DIFF == diff || loop_count > 1);
			// 与前缀的距离过大，diff 无法表达，按 diff 的值线性搜索会太慢，使用尽量快速的搜索
			// 去掉下面这个优化，不会影响解压的结果，但是会降低性能
			// 在 linux 下测试解压 13,034,885 个词条，ZStringTable 比 MultiZStringTable 改善得更明显
			//
			// 单位（毫秒）MultiZStringTable  ZStringTable
			// 使用该优化    14,180            11,740
			// 不用该优化    18,004            12,660
			//
			if (nPrefix == myPrefix)
			{
				if (zstr_t::MAX_DIFF == diff)
				{
					iter.m_iter = lower_bound_near(m_index.begin(),
						iter.m_iter, *iter.m_iter, diff, typename node_t::CompOffset());
				}
			}
		}
		assert(loop_count < 10000); // make sure no dead loop
	}
}

// push_back 元素的顺序必须是字典顺序
// val 包含控制字符(ASCII code range is [1, 32])时会使用一个转义字符(0xFF)
template<class ValueT, class CompareT, uint_t FlagLength, bool IsUniqueKey>
void ZStringTable<ValueT, CompareT, FlagLength, IsUniqueKey>::push_back(const std::string& xkey, const ValueT& val)
{
	check_key(xkey);

	if (empty())
	{
		push_back_impl(xkey, val, 0, 0);
		m_lastKey = xkey;
		return;
	}

	iterator iBack = end() - 1;

	// 必须大于等于最后一个（现有的最大的）元素
	assert(!m_comp(xkey, m_lastKey)); // m_lastKey <= key --> !(key < m_lastKey)

	typename CompareT::prefix_compare comp(CompareT::prefix_compare::common_prefix_length(xkey, m_lastKey));

	if (comp.getPrefix() == xkey.size() && xkey.size() == m_lastKey.size())
	{
		assert(!IsUniqueKey);

		// 字符串相等，不需要再做一个副本，直接使用先前的
		assert(!m_comp(xkey, m_lastKey) && !m_comp(m_lastKey, xkey));
		m_index.push_back(*(m_index.end() - 1));
		(*(m_index.end()-2)).offset = (*(m_index.end()-3)).offset;
		(*(m_index.end()-2)).val()  = val;
		return;
	}

	if (m_comp(xkey, m_lastKey)) // safe check, if violated, it is a fatal error!!
	{
		string_appender<> oss;
		oss << __FILE__ << ":" << __LINE__ << ", key='" << xkey << "', lastKey='" << m_lastKey
			<< "', key must not less than lastKey.\n"
			<< "in function: " << BOOST_CURRENT_FUNCTION << "\n"
			<< "other member[size=" << size()
			<< ", pool[used=" << m_pool.tell() << ", total=" << m_pool.total_size() << "]]";
		fprintf(stderr, "%s\n", oss.str().c_str());
		throw std::logic_error(oss.str().c_str());
	}

	if (comp.getPrefix() <= FlagLength)
	{
		// 前缀很短，不需要压缩
		push_back_impl(xkey, val, 0, 0);
		m_lastKey = xkey;
		return;
	}

	// 前缀最少应和 FlagLength 一样长，
	// 最长即为 common_prefix_length(xkey, m_lastKey)

	iterator iPrefix = iBack - primary_prefix_diff(iBack);

	// 让 diff 总小于等于 zstr_t::MAX_DIFF
	// 如果 end() - iter1 大于 zstr_t::MAX_DIFF，解压时就会多一点开销（顺次解压被引用的前缀）
	// 如果相同前缀的词很多，最后面的词，其 diff 可能就会超出范围，从而发生这种情况
	// 前缀可能跟 key 相同的最小 string（其 iterator 必须在 diff 的表达范围内）
	if (iBack - iPrefix > zstr_t::MAX_DIFF - 1)
	{
#ifdef TERARK_PREFIX_ZIP_ALLOW_CHAIN_ZIP
		iPrefix = iBack - (zstr_t::MAX_DIFF - 1);
#else
	// 为了避免查找前缀，不压缩该字符串
	// 这样做在某些情况下提高了解压效率，但使得压缩率有少许降低（一般小于 1%）
	// 不需要改变解压算法
		push_back_impl(xkey, val, 0, 0);
		m_lastKey = xkey;
		return;
#endif
	}
	iterator iLower = lower_bound(iPrefix, end(), xkey, comp);

	// now [iLower, end) has the same (prefix length = comp.getPrefix())
	uint_t diff = end() - iLower;

	push_back_impl(xkey, val, comp.getPrefix(), diff);
	m_lastKey = xkey;
}

template<class ValueT, class CompareT, uint_t FlagLength, bool IsUniqueKey>
void ZStringTable<ValueT, CompareT, FlagLength, IsUniqueKey>::push_back_impl(
	const std::string& xkey, const value_type& val, uint_t nPreifx, uint_t diff)
{
	uint32_t backOffset = m_pool.put(xkey, nPreifx, diff);

	m_index.back().val() = val;

	uint32_t offset = m_pool.tell();
	m_index.push_back(node_t(offset)); // push a dummy

	// 压缩的尺寸必须比 string 的尺寸小(但key第一个字节为特殊字符除外)
	assert(offset - backOffset <= xkey.size() + 1);
}

#define TERARK_ZSTR_SET_F(CompareT, FlagLength) \
	terark::prefix_zip::ZStringTable<terark::prefix_zip::nil, CompareT, FlagLength, true>

template<class Table1, class Table2>
void copy_to_zstring_table(const Table1& src, Table2& dest)
{
	typename Table1::const_iterator iend = src.end();
	for (typename Table1::const_iterator iter = src.begin(); iter != iend;)
	{
		typename Table1::const_iterator ilower = iter;
		typename Table1::key_type xkey = src.key(ilower);
		size_t valCount = src.goto_next_larger(iter);
		dest.push_back(xkey, ilower, iter, valCount);
	}
}

template<class TableT>
void export_to_zstr_set(const TableT& src, typename TableT::zstr_set& zs)
{
	typename TableT::const_iterator iend = src.end();
	for (typename TableT::const_iterator iter = src.begin(); iter != iend;)
	{
		typename TableT::const_iterator ilower = iter;
		typename std::string xkey = src.key(ilower);
		zs.push_back(xkey, nil());
	}
}


} } // namespace terark::prefix_zip


#endif // __terark_zstring_table_h__

