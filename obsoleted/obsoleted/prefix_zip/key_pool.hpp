/* vim: set tabstop=4 : */
#ifndef __terark_key_pool_h__
#define __terark_key_pool_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

#ifndef __terark_io_DataIO_h__
# include <terark/io/DataIO.hpp>
#endif

namespace terark { namespace prefix_zip {

struct nil {}; // for empty

template<class IndexNodeT, class CompValueT>
struct IndexNode_CompValue : private CompValueT
{
	IndexNode_CompValue() {}
	explicit IndexNode_CompValue(const CompValueT& compVal) : CompValueT(compVal) {}

	bool operator()(const IndexNodeT& left, const IndexNodeT& right) const
	{
		return CompValueT::operator()(left.val(), right.val());
	}
};

template<class KeyT, class ValueT, class KeyCompareT>
class KeyValueNode
{
	KeyT   m_key;
	ValueT m_val;

public:
	typedef KeyT key_id_t;
	BOOST_STATIC_CONSTANT(uint_t, PACK_SIZE = sizeof(ValueT));

	KeyValueNode(const std::pair<KeyT, ValueT>& p)
		: m_key(p.first), m_val(p.second)
	{
	}
	KeyValueNode(const KeyT& key, const ValueT& val)
		: m_key(key), m_val(val)
	{
	}
	KeyValueNode()
	{
	}
	const KeyT&   key() const { return m_key; }
	const ValueT& val() const { return m_val; }

	KeyT&   key() { return m_key; }
	ValueT& val() { return m_val; }

	const KeyT& key_id() const { return m_key; }

	template<class Input > void load_key( Input&  input) {  input >> m_key; }
	template<class Output> void save_key(Output& output) const { output << m_key; }

	struct CompID
	{
		bool operator()(const KeyValueNode& x, const KeyValueNode& y) const
		{
			return KeyCompareT()(x.key(), y.key());
		}
	};
	typedef KeyValueNode type;
};

template<class KeyT>
class KeyPool
{
	uint32_t m_cur_key_count;
	uint32_t m_max_key_count;

public:
	typedef KeyT key_id_t;
	typedef KeyT key_type;
	typedef KeyT raw_key_t; //!< 可以自动转化到 key_type
	typedef KeyPool<KeyT> key_pool_t;

	explicit KeyPool(uint32_t maxKeyPoolSize)
	{
		m_cur_key_count = 0;
		m_max_key_count = (maxKeyPoolSize + sizeof(KeyT) - 1) / sizeof(KeyT);
	}

	uint32_t key_count()  const { return m_cur_key_count; }

	uint32_t used_size()  const { return m_cur_key_count * sizeof(KeyT); }
	uint32_t total_size() const { return m_max_key_count * sizeof(KeyT); }

	void reset() { m_cur_key_count = 0;	}

	const raw_key_t& raw_key(const key_id_t& key) const { return key; }

	template<class NodeT>
	raw_key_t raw_key(const NodeT& node) const
	{
		return node.key();
	}

	const key_id_t& make_key(const KeyT& xkey)
	{
		if (m_cur_key_count >= m_max_key_count)
		{
			string_appender<> oss;
			oss << BOOST_CURRENT_FUNCTION
				<< "key_count[cur=" << m_cur_key_count
				<< ", max=" << m_max_key_count
				<< ", maked=" << m_max_key_count
				<< "]";
			throw OutOfSpaceException(oss.str().c_str());
		}
		m_cur_key_count++;
		return xkey;
	}
	void unmake_key(const KeyT& xkey)
	{
		m_cur_key_count--;
	}

	bool can_insert(const KeyT& xkey) const
	{
		return m_cur_key_count < m_max_key_count;
	}

	void init(uint32_t newSize)
	{
		m_max_key_count = (newSize + sizeof(KeyT) - 1) / sizeof(KeyT);
		m_cur_key_count = 0;
	}

	void setMaxPoolSize(uint32_t newSize)
	{
		m_max_key_count = (newSize + sizeof(KeyT) - 1) / sizeof(KeyT);
	}

	void seek(uint32_t newPos)
	{
		assert(newPos / sizeof(KeyT) <= m_max_key_count);
		m_cur_key_count = newPos / sizeof(KeyT);
	}

	DATA_IO_LOAD_SAVE_V(KeyPool, 1,
		& m_max_key_count
		& m_cur_key_count
		)
};

} } // namespace terark::prefix_zip

#endif // __terark_key_pool_h__
