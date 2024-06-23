/* vim: set tabstop=4 : */
#ifndef __terark_bitvector_h__
#define __terark_bitvector_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

#include <vector>
#include "io/DataIO.hpp"

namespace terark {

template<class BaseVector>
class bitvector
{
	typedef typename BaseVector::value_type base_value_type;
	BOOST_STATIC_CONSTANT(int, base_bits = sizeof(base_value_type)*8);

public:
	typedef bool	value_type;
	typedef bool	reference;

	typedef typename BaseVector::difference_type difference_type;
	typedef typename BaseVector::size_type       size_type;

public:
	bitvector()
	{
		m_size = 0;
	}

	explicit bitvector(size_type size, bool val = false)
		: m_blocks(block_count(size), val ? ~base_value_type(0) : base_value_type(0))
	{
		m_size = size;
	}

	bool operator[](difference_type idx) const
	{
		assert(0 <= idx && idx < difference_type(m_size));
		return !!(m_blocks[idx / base_bits] & (base_value_type(1) << idx % base_bits));
	}
	bool at(difference_type idx) const
	{
		if (0 <= idx && idx < difference_type(m_size))
			return operator[](idx);
		else
			throw std::out_of_range(BOOST_CURRENT_FUNCTION);
	}
	void unchecked_set(difference_type idx)
	{
		m_blocks[idx / base_bits] |= (base_value_type(1) << idx % base_bits);
	}
	void unchecked_reset(difference_type idx)
	{
		m_blocks[idx / base_bits] &= ~(base_value_type(1) << idx % base_bits);
	}
	void unchecked_set(difference_type idx, bool val)
	{
		val ? unchecked_set(idx) : unchecked_reset(idx);
	}
	void set(difference_type idx)
	{
		if (0 <= idx && idx < difference_type(m_size))
			unchecked_set(idx);
		else
			throw std::out_of_range(BOOST_CURRENT_FUNCTION);
	}
	void reset(difference_type idx)
	{
		if (0 <= idx && idx < difference_type(m_size))
			unchecked_reset(idx);
		else
			throw std::out_of_range(BOOST_CURRENT_FUNCTION);
	}
	void set(difference_type idx, bool val)
	{
		if (0 <= idx && idx < difference_type(m_size))
			val ? unchecked_set(idx) : unchecked_reset(idx);
		else
			throw std::out_of_range(BOOST_CURRENT_FUNCTION);
	}
	void fill(bool val)
	{
		std::fill(m_blocks.begin(), m_blocks.end(), base_value_type(val?~0:0));
	}

	void add_true(difference_type idx)
	{
		if (idx < 0)
			throw std::out_of_range(BOOST_CURRENT_FUNCTION);

		if (idx >= difference_type(m_blocks.size() * base_bits))
		{
			size_t newBlocks = block_count(idx+1);
			m_blocks.resize(newBlocks + newBlocks/2);
		}
		if (difference_type(m_size) <= idx)
			m_size = idx + 1;

		unchecked_set(idx);
	}

	void add_false(difference_type idx)
	{
		if (idx < 0)
			throw std::out_of_range(BOOST_CURRENT_FUNCTION);

		if (idx >= difference_type(m_blocks.size() * base_bits))
		{
			size_t newBlocks = block_count(idx+1);
			m_blocks.resize(newBlocks + newBlocks/2);
		}
		if (difference_type(m_size) <= idx)
			m_size = idx + 1;
	//	unchecked_reset(idx); // not needed
	}

	void add_val(difference_type idx, bool val)
	{
		if (idx < 0)
			throw std::out_of_range(BOOST_CURRENT_FUNCTION);

		if (idx >= difference_type(m_blocks.size() * base_bits))
		{
			size_t newBlocks = block_count(idx+1);
			m_blocks.resize(newBlocks + newBlocks/2);
		}
		if (difference_type(m_size) <= idx)
			m_size = idx + 1;
		unchecked_set(idx, val);
	}

	/**
	 @brief test bit value

	 @note
	  if idx out of range, return false
	 */
	bool test(difference_type idx) const
	{
		if (idx >= difference_type(m_size))
			return false;
		return operator[](idx);
	}

	void push_back(bool val)
	{
		if (m_size >= m_blocks.size() * base_bits)
		{
			m_blocks.push_back(base_value_type(0));
			m_blocks.resize(m_blocks.capacity());
		}
		unchecked_set(m_size++, val);
	}

	void reserve(size_type capacity)
	{
		m_blocks.reserve(block_count(capacity));
	}

	void resize(size_type size)
	{
		m_blocks.resize(block_count(size));
		m_size = size;
	}

	size_type size()     const { return m_size; }
	size_type capacity() const { return m_blocks.size() * base_bits; }

	base_value_type* block_buffer() { return &*m_blocks.begin(); }
	const base_value_type* block_buffer() const { return &*m_blocks.begin(); }
	size_type block_count() const { return m_blocks.size(); }

	static size_type block_count(size_type nbits) { return (nbits+base_bits-1)/base_bits; }

	DATA_IO_LOAD_SAVE(bitvector, & m_size & m_blocks);

private:
	template<class DataIO>
	friend void DataIO_dump_load_object(DataIO& dio, bitvector& bv)
	{
		uint32_t size;
		dio >> size;
		if (size > bv.m_size)
			bv.resize(size);
		const uint32_t e_size = sizeof(typename BaseVector::value_type);
		dio.ensureRead(&*bv.m_blocks.begin(), e_size*bitvector::block_count(size));
	}
	template<class DataIO>
	friend void DataIO_dump_save_object(DataIO& dio, const bitvector& bv)
	{
		const uint32_t e_size = sizeof(typename BaseVector::value_type);
		dio << uint32_t(bv.m_size);
		dio.ensureWrite(&*bv.m_blocks.begin(), e_size*bv.m_blocks.size());
	}
	uint32_t m_size;
	BaseVector m_blocks;
};

template<class BaseVector>
class bitmatrix
{
	typedef bitvector<BaseVector> bitv_t;
public:
	typedef typename BaseVector::value_type value_type;

	bitmatrix(int row, int col)
	{
		resize(row, col);
	}
	void resize(int row, int col)
	{
		m_row = row;
		m_col = col;
		m_bv.resize(row*col);
	}
	bool at(int i, int j) const
	{
		assert(i >= 0);
		assert(j >= 0);
		if (i >= m_row)
			throw std::out_of_range(BOOST_CURRENT_FUNCTION);
		if (j >= m_col)
			throw std::out_of_range(BOOST_CURRENT_FUNCTION);
		return m_bv[i*m_col+j];
	}
	bool operator()(int i, int j) const
	{
		return at(i, j);
	}
	void set(int i, int j)
	{
		assert(i >= 0);
		assert(j >= 0);
		if (i >= m_row)
			throw std::out_of_range(BOOST_CURRENT_FUNCTION);
		if (j >= m_col)
			throw std::out_of_range(BOOST_CURRENT_FUNCTION);
		m_bv.set(i*m_col+j);
	}
	void reset(int i, int j)
	{
		assert(i >= 0);
		assert(j >= 0);
		if (i >= m_row)
			throw std::out_of_range(BOOST_CURRENT_FUNCTION);
		if (j >= m_col)
			throw std::out_of_range(BOOST_CURRENT_FUNCTION);
		m_bv.reset(i*m_col+j);
	}
	void fill(bool val) { m_bv.fill(val); }

protected:
	int m_row, m_col;
	bitv_t m_bv;

	template<class DataIO>
	friend void DataIO_dump_load_object(DataIO& dio, bitmatrix& bm)
	{
		dio >> bm.m_row >> bm.m_col;
		DataIO_dump_load_object(dio, bm.m_bv);
	}
	template<class DataIO>
	friend void DataIO_dump_save_object(DataIO& dio, const bitmatrix& bm)
	{
		dio << bm.m_row << bm.m_col;
		DataIO_dump_save_object(dio, bm.m_bv);
	}
	DATA_IO_LOAD_SAVE(bitmatrix, & m_row & m_col & m_bv);
};

template<class BaseVector>
class bit_triangle
{
	typedef bitvector<BaseVector> bitv_t;
public:
	typedef typename BaseVector::value_type value_type;

	bit_triangle(int n)
	{
		resize(n);
	}
	void resize(int n)
	{
		m_n = n;
		m_bv.resize(n*(n+1)/2);
	}
	bool at(int i, int j) const
	{
		assert(i >= 0);
		assert(j >= 0);
		if (i >= m_n)
			throw std::out_of_range(BOOST_CURRENT_FUNCTION);
		if (j >= m_n)
			throw std::out_of_range(BOOST_CURRENT_FUNCTION);
		if (i < j)
			return m_bv[pos(i,j)];
		else
			return m_bv[pos(j,i)];
	}
	bool operator()(int i, int j) const
	{
		return at(i, j);
	}
	void set(int i, int j)
	{
		assert(i >= 0);
		assert(j >= 0);
		if (i >= m_n)
			throw std::out_of_range(BOOST_CURRENT_FUNCTION);
		if (j >= m_n)
			throw std::out_of_range(BOOST_CURRENT_FUNCTION);
		if (i < j)
			m_bv.set(pos(i,j));
		else
			m_bv.set(pos(j,i));
	}
	void reset(int i, int j)
	{
		assert(i >= 0);
		assert(j >= 0);
		if (i >= m_n)
			throw std::out_of_range(BOOST_CURRENT_FUNCTION);
		if (j >= m_n)
			throw std::out_of_range(BOOST_CURRENT_FUNCTION);
		if (i < j)
			m_bv.reset(pos(i,j));
		else
			m_bv.reset(pos(j,i));
	}
	void fill(bool val) { m_bv.fill(val); }

protected:
	int pos(int i, int j) const
	{
		assert(j >= i);
		return (2*m_n-i+1)*i/2 + j-i;
	}

	int m_n;
	bitv_t m_bv;

	template<class DataIO>
	friend void DataIO_dump_load_object(DataIO& dio, bit_triangle& bt)
	{
		dio >> bt.m_n;
		DataIO_dump_load_object(dio, bt.m_bv);
	}
	template<class DataIO>
	friend void DataIO_dump_save_object(DataIO& dio, const bit_triangle& bt)
	{
		dio << bt.m_n;
		DataIO_dump_save_object(dio, bt.m_bv);
	}
	DATA_IO_LOAD_SAVE(bit_triangle, & m_n & m_bv);
};

} // namespace terark

#endif // __terark_bitvector_h__
