/* vim: set tabstop=4 : */
#ifndef __terark_value_zipped_table_h__
#define __terark_value_zipped_table_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

#include <stdexcept>

#include "ext_index.hpp"

#include "../DataBuffer.hpp"
#include "../string_compare.hpp"
#include <terark/io/DataIO.hpp>
#include <terark/io/MemStream.hpp>

#include <boost/operators.hpp>
//#include <boost/iterator.hpp>
#include <boost/iterator/reverse_iterator.hpp>
#include <boost/static_assert.hpp>

#include "dump_header.hpp"

namespace terark { namespace prefix_zip {

//typedef stream_position_t zipped_value_position_t;
typedef uint32_t zipped_value_position_t;

struct ValueZippedTable_dump_header : public Table_dump_header
{
	uint64_t streamSize;

	ValueZippedTable_dump_header()
	{
		version = 0;
		keyCount = 0;
		valCount = 0;
		streamSize = 0;
	}

	DATA_IO_VERSIONED_LOAD_SAVE(ValueZippedTable_dump_header, 1,
		& vmg.template base<Table_dump_header>(this)
		& streamSize
		)
};

template<class ValueT>
class NonZipCoder
{
public:
	typedef ValueT    value_type;

	template<class Output, class ForwIter>
	size_t write(Output& output, ForwIter first, ForwIter last)
	{
		size_t count = 0;
		for (; first != last; ++first, ++count)
			output << *first;
		return count;
	}

	template<class Input>
	void read(Input& input)
	{
		input >> m_val;
	}

	template<class Input>
	void read_block(Input& input)
	{
		input >> m_val;
	}

	template<class Input, class OutIter>
	void decode(Input& input, size_t bytes, OutIter dest)
	{
		stream_position_t old_pos = input.getStream().tell();
		while (input.getStream().tell() < old_pos + bytes)
		{
			input >> *dest++;
		}
	}

	const value_type& val() const { return m_val; }

private:
	value_type m_val;
};

class uint_dpcm_encoder
{
	unsigned m_prevVal;

	template<class Output>
	void write_one(Output& output, unsigned val)
	{
		var_uint32_t diff(val - m_prevVal);
		output << diff;
		m_prevVal = val;
	}
public:

	template<class Output>
	void write(Output& output, unsigned val)
	{
		assert(val >= m_prevVal);
		write_one(output, val);
	}
	template<class Output, class IterT>
	size_t write(Output& output, const IterT& first, const IterT& last)
	{
		size_t count = 0;
		for (IterT iter = first; iter != last; ++iter, ++count)
		{
			write_one(output, *iter);
		}
		return count;
	}
};

class uint_dpcm_decoder
{
	unsigned m_curVal;

public:
	/**
	 @brief 简易说明
	 读取一个块，并解析出这个块的第一个 value
	 */
	template<class Input>
	void read_block(Input& input)
	{
		input >> m_curVal;
	}

	template<class Input>
	void read(Input& input)
	{
		var_uint32_t diff;
		input >> diff;
		m_curVal += diff.t;
	}
	unsigned val() const { return m_curVal; }
};

template<class BaseTableT, class ValueEncoder, class ValueDecoder, class Input, class Output>
class ValueZippedTable;
/**
 @brief 简易说明

 */
template<class BaseTableT,
		 class ValueEncoder,
		 class ValueDecoder,
		 class Input,
		 class Output,
		 class IndexIter
>
class ValueZippedTable_ConstIterator :
		public boost::forward_iterator_helper<
			ValueZippedTable_ConstIterator<BaseTableT, ValueEncoder, ValueDecoder, Input, Output, IndexIter>,
			typename ValueEncoder::value_type,
			std::ptrdiff_t,
			const typename ValueEncoder::value_type*,
			const typename ValueEncoder::value_type>
{
	friend class ValueZippedTable<BaseTableT, ValueEncoder, ValueDecoder, Input, Output>;
	typedef ValueZippedTable<BaseTableT, ValueEncoder, ValueDecoder, Input, Output> owner_t;
	typedef ValueZippedTable_ConstIterator  my_type;

public:
	typedef ValueEncoder   encoder_t;
	typedef ValueDecoder   decoder_t;
	typedef typename Input::stream_t         stream_t;
	typedef typename ValueEncoder::value_type value_type;

private:
	const owner_t*	m_owner;
	decoder_t		m_decoder;
	stream_t		m_stream; // only save private position state, data is in m_owner->m_stream
	Input			m_input;
	IndexIter		m_iter;
	size_t			m_val_index;

public:
	ValueZippedTable_ConstIterator()
	{
		m_owner = 0;
		m_input.attach(&m_stream);
	}

	ValueZippedTable_ConstIterator(const ValueZippedTable_ConstIterator& that)
	{
		m_val_index	= that.m_val_index;
		m_owner		= that.m_owner;
		m_decoder	= that.m_decoder;
		m_stream.duplicate(that.m_stream);
		m_input		= that.m_input;
		m_iter		= that.m_iter;
	}

	value_type operator*() const { return m_decoder.val(); }

	my_type& operator++()
	{
		assert(!is_end());

		++m_val_index;
		if (m_val_index == m_iter[1].val_index) {
			m_decoder.read_block(m_input);
			++m_iter;
		} else
			m_decoder.read(m_input);

		return *this;
	}

	friend bool operator==(const ValueZippedTable_ConstIterator& x, const ValueZippedTable_ConstIterator& y)
	{
		assert(x.m_owner == y.m_owner);
		return x.m_iter == y.m_iter && x.m_val_index == y.m_val_index;
	}

	bool equal_next() const
	{
		assert(!is_end());
		return m_val_index + 1 < m_iter[1].val_index; // !! must be "m_val_index + 1"
	}

	std::ptrdiff_t distance(const ValueZippedTable_ConstIterator& larger)
	{
		std::ptrdiff_t dist = 0;
		ValueZippedTable_ConstIterator iter(*this);
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
	//	assert(0 == m_val_index);

		size_t equal_count = m_iter[1].val_index - m_iter->val_index;
		++m_iter;
		m_val_index = *m_iter;

		return equal_count;
	}

	size_t equal_count() const { return m_iter[1].val_index - m_iter->val_index; }

private:
	ValueZippedTable_ConstIterator(const owner_t* owner, IndexIter ibig)
	{
		m_input.attach(&m_stream);
		m_owner = owner;
		m_iter = ibig;
		m_val_index = 0;
	}

	bool is_end() const
	{
		return m_iter->val_index == m_owner->m_index.end()->val_index;
	}

	typename BaseTableT::key_type key() const { return m_owner->key(*this); }
};

template<class BaseTableT, class ValueEncoder, class ValueDecoder, class Input, class Output>
class ValueZippedTable
{
public:
	typedef Input    input_t;
	typedef Output   output_t;
	typedef typename Input::stream_t          stream_t;
	typedef typename BaseTableT::compare_t    compare_t;
	typedef typename BaseTableT::key_type     key_type;
	typedef typename ValueEncoder::value_type value_type;

	BOOST_STATIC_ASSERT(stream_t::is_seekable::value);
	BOOST_STATIC_ASSERT((boost::is_same<typename Input ::stream_t,
										typename Output::stream_t>::value));

	typedef ValueZippedTable_dump_header dump_header;

	struct ValueSetHeader
	{
		zipped_value_position_t offset;
		uint32_t val_index;

		explicit ValueSetHeader(zipped_value_position_t offset = 0, uint32_t val_index = 0)
		{
			this->offset = offset;
			this->val_index = val_index;
		}
	};
	typedef typename ExtIndexType<BaseTableT, ValueSetHeader>::index_t index_t;

	stream_t     m_stream;
	output_t	 m_output;
	index_t      m_index;
	key_type     m_lastKey;
	ValueEncoder m_encoder;

public:
	friend class ValueZippedTable_ConstIterator<
				BaseTableT,
				ValueEncoder,
				ValueDecoder,
				Input,
				Output,
				typename index_t::const_iterator
			>;
	typedef ValueZippedTable_ConstIterator<
				BaseTableT,
				ValueEncoder,
				ValueDecoder,
				Input,
				Output,
				typename index_t::const_iterator
			>
			const_iterator;
	typedef const_iterator iterator; // ValueZippedTable don't has non-const iterator

	typedef std::pair<iterator, iterator>					range_t;
	typedef std::pair<const_iterator, const_iterator> const_range_t;

	typedef size_t size_type;

	explicit ValueZippedTable(size_t maxPoolSize, compare_t comp = compare_t())
		: m_index(maxPoolSize, comp)
	{}

	compare_t comp() const { return m_index.comp(); }

	stream_t& stream() { return m_stream; }

	const_iterator begin() const { return const_iterator(this, m_index.begin()); }
	const_iterator end() const { return const_iterator(this, m_index.end()); }

	size_type size() const { return m_index.end()->val_index; }

	key_type key(const_iterator iter) const { return m_index.key(iter.m_iter); }

	const key_type& lastKey() const { return m_lastKey; }

	template<class CompatibleCompare>
	const_iterator lower_bound(const key_type& key, CompatibleCompare comp) const
	{
		return const_iterator(this, m_index.lower_bound(key, comp));
	}

	template<class CompatibleCompare>
	const_iterator upper_bound(const key_type& key, CompatibleCompare comp) const
	{
		return const_iterator(this, m_index.upper_bound(key, comp));
	}

	template<class CompatibleCompare>
	const_range_t equal_range(const key_type& key, CompatibleCompare comp) const
	{
		std::pair<typename const_iterator::big_iter_t,
				  typename const_iterator::big_iter_t>
			ii = m_index.equal_range(key, comp);

		return std::pair<const_iterator, const_iterator>(
					const_iterator(this, ii.first),
					const_iterator(this, ii.second));
	}

	/**
	 @brief
	  只能一次批量插入 (key-value_set)，并且每次的 key 必不同且大于前一个
	 */
	template<class IterT>
	void push_back(const key_type& key, IterT first, IterT last, int count)
	{
		size_t old_size = size();
		push_back(key, first, last);
		m_lastKey = key;
		TERARK_RT_assert(old_size + count == size(), std::invalid_argument);
	}
	template<class IterT>
	void push_back(const key_type& key, IterT first, IterT last)
	{
		size_t old_size = size();
		m_index.push_back(key, ValueSetHeader(m_stream.tell(), old_size));
		size_t count = m_encoder.write(m_output, first, last);
		(*m_index.end()).offset = m_stream.tell();
		(*m_index.end()).val_index = old_size + count;

		m_lastKey = key;
	}
	void push_back(const key_type& key, const value_type& val)
	{
		assert(!comp()(key, m_index.lastKey())); // key >= lastKey

		if (comp()(m_index.lastKey(), key))
			m_index.push_back(ValueSetHeader(m_stream.tell(), size()));
		else
			m_index.end()->val_index++;

		m_encoder.write(m_output, val);

		m_index.end()->offset = m_stream.tell();
		m_index.end()->val_index = m_index.back().val_index + 1;

		m_lastKey = key;
	}
	void push_back(const value_type& val)
	{
		m_encoder.write(m_output, val);
		m_index.end()->offset = m_stream.tell();
		m_index.end()->val_index++;
	}

	bool equal_next(const const_iterator& iter) const { return iter.equal_next(); }

	size_t used_pool_size()  const { return m_index.end()->offset; }
	size_t total_pool_size() const { return m_stream.size(); }
	size_t available_pool_size() const { return total_pool_size() - used_pool_size(); }
};

} } // namespace terark::prefix_zip


#endif // __terark_value_zipped_table_h__

