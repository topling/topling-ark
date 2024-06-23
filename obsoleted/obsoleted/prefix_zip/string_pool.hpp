/* vim: set tabstop=4 : */
#ifndef __terark_string_pool_h__
#define __terark_string_pool_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

#include <boost/type_traits/is_empty.hpp>
#include <terark/io/MemStream.hpp>
#include <terark/io/DataIO.hpp>
#include "key_pool.hpp"

namespace terark { namespace prefix_zip {

template<class IndexNodeT>
struct IndexNode_CompOffset
{
	bool operator()(const IndexNodeT& left, const IndexNodeT& right) const
	{
		return left.offset < right.offset;
	}
};

template<class ValueT, bool IsEmpty>
struct IndexNode_Aux
{
private:
	ValueT m_val;

public:
	typedef uint32_t key_id_t;
	typedef IndexNode_CompOffset<IndexNode_Aux> CompOffset;
	typedef CompOffset CompID;
	BOOST_STATIC_CONSTANT(uint_t, PACK_SIZE = sizeof(uint32_t) + sizeof(ValueT));

	ValueT& val() { return m_val; }
	const ValueT& val() const { return m_val; }

	uint32_t offset;

	IndexNode_Aux(uint32_t offset, const ValueT& val) : offset(offset), m_val(val) {}

	explicit IndexNode_Aux(uint32_t offset) : offset(offset), m_val() {} // for numeric value, init as 0

	IndexNode_Aux() {} // init as undefined, for serialize from input

	key_id_t key_id() const { return offset; }

	template<class Input > void load_key( Input&  input) {  input >> offset; }
	template<class Output> void save_key(Output& output) const { output << offset; }

	DATA_IO_LOAD_SAVE(IndexNode_Aux, & m_val & offset)
};

template<class ValueT>
struct IndexNode_Aux<ValueT, true> : public ValueT
{
public:
	typedef uint32_t key_id_t; // offset type
	typedef IndexNode_CompOffset<IndexNode_Aux> CompOffset;
	typedef CompOffset CompID;
	BOOST_STATIC_CONSTANT(uint_t, PACK_SIZE = sizeof(uint32_t));

	ValueT& val() { return *this; }
	const ValueT& val() const { return *this; }

	uint32_t offset;

	IndexNode_Aux(uint32_t offset, const ValueT& val) : offset(offset) {}

	explicit IndexNode_Aux(uint32_t offset) : offset(offset) {}

	IndexNode_Aux() {} // init as undefined, for serialize from input

	key_id_t key_id() const { return offset; }

	DATA_IO_LOAD_SAVE(IndexNode_Aux, & offset)
};

/**
 @brief 用于 PackedTable, ZStringTable, StringTable 的内部对象

 只是为 ValueT 增加了一个 offset
 */
template<class ValueT>
struct IndexNode
{
	typedef IndexNode_Aux<ValueT, boost::is_empty<ValueT>::value> type;
};

/**
 @ingroup prefix_zip
 @brief 紧缩字符串池

  所有字符串一个挨一个存储，每个字符串均已 0 结束
 */
class TERARK_DLL_EXPORT StringPool : public AutoGrownMemIO
{
	uint32_t m_key_count;
public:
	typedef uint32_t key_id_t; //!< offset is id for key
	typedef const char* raw_key_t; //!< 可以自动转化到 key_type，但不必从 key_type 自动转化回来

	explicit StringPool(uint32_t maxSize) : AutoGrownMemIO(maxSize), m_key_count(0) { }

	/**
	 @name 与 KeyPool 兼容的接口
	 @{
	 */
	/**
	 @brief 讲字符串放入池中

	  如果池中放不下新字符串，则抛出 OutOfSpaceException 异常
	  if the pool is to be full, throw OutOfSpaceException
	 */
	key_id_t make_key(const std::string& str);
	key_id_t make_key(raw_key_t szstr);
	key_id_t make_key(raw_key_t szstr, int nstr);

	void unmake_key(const std::string& key);

	raw_key_t raw_key(key_id_t offset) const
	{
		assert(offset >= 0 && offset < this->tell());
		return (const char*)(this->buf() + offset);
	}
	template<class NodeT>
	raw_key_t raw_key(const NodeT& node) const
	{
		return raw_key(node.offset);
	}

	bool can_insert(const std::string& str) const
	{
		return AutoGrownMemIO::remain() > str.size();
	}

	void reset() { AutoGrownMemIO::rewind(); m_key_count = 0; }

	uint32_t  key_count() const { return m_key_count; }

	uint32_t  used_size() const { return AutoGrownMemIO::tell(); }
	uint32_t total_size() const { return AutoGrownMemIO::size(); }

	void setMaxPoolSize(uint32_t newSize)
	{
		AutoGrownMemIO::resize(newSize);
	}

	template<class Input>
	void load(Input& input)
	{
		uint32_t used_size, total_size;
		input >> used_size >> total_size;
		AutoGrownMemIO::init(total_size);
		input.ensureRead(AutoGrownMemIO::buf(), used_size);
		AutoGrownMemIO::seek(used_size);
	}
	template<class Output>
	void save(Output& output) const
	{
		uint32_t used_size = AutoGrownMemIO::tell();
		uint32_t total_size = AutoGrownMemIO::size();
		output << used_size << total_size;
		output.ensureWrite(AutoGrownMemIO::buf(), used_size);
	}
	DATA_IO_REG_LOAD_SAVE(StringPool)
	//@}

	/**
	 @brief 字符串提取器

	  用于 multi_index，IndexNode<ValueT>::type::offset 提取出偏移，再到其所在的池中取出字符串
	 */
	template<class ValueT> class KeyExtractor
	{
	public:
		typedef raw_key_t result_type;

		KeyExtractor(const StringPool* pool) : m_key_pool(pool) {}

		raw_key_t operator()(const typename IndexNode<ValueT>::type& node) const
		{
			return m_key_pool->raw_key(node.offset);
		}
	private:
		const StringPool* m_key_pool;
	};
};

//////////////////////////////////////////////////////////////////////////

} } // namespace terark::prefix_zip

#endif // __terark_string_pool_h__

