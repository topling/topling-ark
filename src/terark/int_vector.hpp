#pragma once

#include "bitmap.hpp"
#include <terark/util/throw.hpp>
#include <array>

namespace terark {

class OutputBuffer;

// memory layout is binary compatible to SortedUintVec on 64 bit platform
class TERARK_DLL_EXPORT UintVecMin0Base {
protected:
	valvec<byte> m_data; // corresponding to SortedUintVec::m_data
	size_t m_bits; // corresponding to SortedUintVec::XXXX, m_bits <= 64
	size_t m_mask; // corresponding to SortedUintVec::m_index
	size_t m_size; // corresponding to SortedUintVec::m_size

	void nullize() {
		m_bits = 0;
		m_mask = 0;
		m_size = 0;
	}

public:
	template<class Uint>
	UintVecMin0Base(size_t num, Uint max_val) {
		m_bits = 0;
		m_mask = 0;
		m_size = 0;
		resize_with_wire_max_val(num, max_val);
	}
	UintVecMin0Base() { nullize(); }
	UintVecMin0Base(const UintVecMin0Base&);
	UintVecMin0Base& operator=(const UintVecMin0Base&);
	UintVecMin0Base(UintVecMin0Base&&) = default;
	UintVecMin0Base& operator=(UintVecMin0Base&&) = default;

	~UintVecMin0Base();

	void clear();

	void shrink_to_fit();
	void swap(UintVecMin0Base&);
	void risk_release_ownership();
	void risk_destroy(MemType);
	void risk_set_data(byte* Data, size_t num, size_t bits);

	const byte* data() const { return m_data.data(); }
	size_t uintbits() const { return m_bits; }
	size_t uintmask() const { return m_mask; }
	size_t size() const { return m_size; }
	size_t mem_size() const { return m_data.size(); }

	void resize(size_t newsize);

	void set_wire(size_t idx, size_t val) {
		assert(idx < m_size);
		assert(val <= m_mask);
		assert(m_bits <= 64);
		size_t bits = m_bits; // load member into a register
		size_t bit_idx = bits * idx;
		febitvec::s_set_uint((size_t*)m_data.data(), bit_idx, bits, val);
	}

	template<class Uint>
	void resize_with_wire_max_val(size_t num, Uint max_val) {
		BOOST_STATIC_ASSERT(boost::is_unsigned<Uint>::value);
	//	assert(max_val > 0);
		assert(m_bits <= 64);
		size_t bits = compute_uintbits(max_val);
		resize_with_uintbits(num, bits);
	}

	void resize_with_uintbits(size_t num, size_t bit_width);

private:
	terark_no_inline void push_back_slow_path(size_t val);
public:
	void push_back(size_t val) {
		assert(m_bits <= 64);
		if (compute_mem_size(m_bits, m_size+1) < m_data.size() && val <= m_mask) {
            set_wire(m_size++, val);
		} else {
			push_back_slow_path(val);
		}
	}

	static size_t compute_mem_size(size_t bits, size_t num) {
		assert(bits <= 64);
		size_t usingsize = (bits * num + 7)/8;
		size_t touchsize =  usingsize + sizeof(uint64_t)-1;
		size_t alignsize = (touchsize + 15) &  ~size_t(15); // align to 16
		return alignsize;
	}

    static size_t compute_uintbits(size_t value) {
        size_t bits = 0;
        while (value) {
            bits++;
            value >>= 1;
        }
        return bits;
    }

    static size_t compute_mem_size_by_max_val(size_t max_val, size_t num) {
      size_t bits = compute_uintbits(max_val);
      return compute_mem_size(bits, num);
    }

	template<class Int>
	Int build_from(const valvec<Int>& y) { return build_from(y.data(), y.size()); }
	template<class Int>
	Int build_from(const Int* src, size_t num) {
		BOOST_STATIC_ASSERT(sizeof(Int) <= sizeof(size_t));
		assert(m_bits <= 64);
		if (0 == num) {
			clear();
			return 0;
		}
		assert(NULL != src);
		Int min_val = src[0];
		Int max_val = src[0];
		for (size_t i = 1; i < num; ++i) {
			if (max_val < src[i]) max_val = src[i];
			if (min_val > src[i]) min_val = src[i];
		}
		typedef typename boost::make_unsigned<Int>::type Uint;
		ullong wire_max = Uint(max_val - min_val);
		resize_with_wire_max_val(num, wire_max);
		for (size_t i = 0; i < num; ++i)
			set_wire(i, Uint(src[i] - min_val));
		return min_val;
	}

    class TERARK_DLL_EXPORT Builder {
    public:
        struct BuildResult {
            size_t uintbits;    // UintVecMin0::uintbits()
            size_t size;        // UintVecMin0::size()
            size_t mem_size;    // UintVecMin0::mem_size()
        };
        virtual ~Builder();
        virtual void push_back(size_t value) = 0;
        virtual BuildResult finish() = 0;
    };
    static Builder* create_builder_by_uintbits(size_t uintbits, const char* fpath);
    static Builder* create_builder_by_max_value(size_t max_val, const char* fpath);
    static Builder* create_builder_by_uintbits(size_t uintbits, OutputBuffer* buffer);
    static Builder* create_builder_by_max_value(size_t max_val, OutputBuffer* buffer);
};

// Max uint bits is 58
/*
#if TERARK_WORD_BITS == 64
// allowing bits > 58 will incure performance punish in get/set.
// 58 bit can span 9 bytes, but this only happens when start bit index
// is odd, 58 is not odd, so 58 is safe.
#endif
*/
class UintVecMin0 : public UintVecMin0Base {
public:
  using UintVecMin0Base::UintVecMin0Base;
  // NOLINTNEXTLINE
  void risk_set_data(byte* Data, size_t num, size_t bits) {
#if TERARK_WORD_BITS == 64
    // allowing bits > 58 will incur performance punish in get/set.
    // 58 bit can span 9 bytes, but this only happens when start bit index
    // is odd, 58 is not odd, so 58 is safe.
    if (bits > 58) {
      assert(false);
      THROW_STD(logic_error, "bits=%zd is too large(max_allowed=58)", bits);
    }
#endif
    UintVecMin0Base::risk_set_data(Data, num, bits);
  }
  size_t get(size_t idx) const {
    assert(idx < m_size);
    assert(m_bits <= 58);
#if defined(__BMI2__)
    size_t bits = m_bits;
    size_t bit_idx = bits * idx;
    size_t byte_idx = bit_idx / 8;
    size_t val = unaligned_load<size_t>(m_data.data() + byte_idx);
    return _bextr_u64(val, bit_idx % 8, unsigned(bits));
#else
    return fast_get(m_data.data(), m_bits, m_mask, idx);
#endif
  }
  void get2(size_t idx, size_t aVals[2]) const {
    const byte*  data = m_data.data();
    const size_t bits = m_bits;
    const size_t mask = m_mask;
    assert(m_bits <= 58);
    aVals[0] = fast_get(data, bits, mask, idx);
    aVals[1] = fast_get(data, bits, mask, idx+1);
  }
  std::array<size_t,2> get2(size_t idx) const {
    const byte*  data = m_data.data();
    const size_t bits = m_bits;
    const size_t mask = m_mask;
    assert(m_bits <= 58);
	std::array<size_t,2> aVals;
    aVals[0] = fast_get(data, bits, mask, idx);
    aVals[1] = fast_get(data, bits, mask, idx+1);
	return aVals;
  }
  static
  size_t fast_get(const byte* data, size_t bits, size_t mask, size_t idx) {
    assert(bits <= 58);
    size_t bit_idx = bits * idx;
    size_t byte_idx = bit_idx / 8;
    size_t val = unaligned_load<size_t>(data + byte_idx);
    return (val >> bit_idx % 8) & mask;
  }
  static void fast_prefetch(const byte* data, size_t bits, size_t idx) {
    assert(bits <= 58);
    size_t bit_idx = bits * idx;
    size_t byte_idx = bit_idx / 8;
    _mm_prefetch((const char*)data + byte_idx, _MM_HINT_T0);
  }
  void prefetch(size_t idx) const {
    assert(idx < m_size);
    assert(m_bits <= 58);
    fast_prefetch(m_data.data(), m_bits, idx);
  }
  size_t back() const { assert(m_size > 0); return get(m_size-1); }
  size_t operator[](size_t idx) const { return get(idx); }

  std::pair<size_t, size_t>
         equal_range(size_t lo, size_t hi, size_t key) const noexcept;
  size_t lower_bound(size_t lo, size_t hi, size_t key) const noexcept;
  size_t upper_bound(size_t lo, size_t hi, size_t key) const noexcept;

  std::pair<size_t, size_t>
         equal_range(size_t key) const noexcept { return equal_range(0, m_size, key); }
  size_t lower_bound(size_t key) const noexcept { return lower_bound(0, m_size, key); }
  size_t upper_bound(size_t key) const noexcept { return upper_bound(0, m_size, key); }
};

// Max uint bits is 64
class BigUintVecMin0 : public UintVecMin0Base {
public:
  using UintVecMin0Base::UintVecMin0Base;
  size_t get(size_t idx) const {
    assert(idx < m_size);
    assert(m_bits <= 64);
    size_t bits = m_bits;
    size_t pos = bits * idx;
    size_t val = febitvec::s_get_uint((const size_t*)m_data.data(), pos, bits);
    assert(val <= m_mask);
    return val;
  }
  void get2(size_t idx, size_t aVals[2]) const {
    const size_t* data = (const size_t*)m_data.data();
    const size_t  bits = m_bits;
    const size_t  pos = bits * idx;
    assert(bits <= 64);
    aVals[0] = febitvec::s_get_uint(data, pos, bits);
    aVals[1] = febitvec::s_get_uint(data, pos + bits, bits);
  }
  std::array<size_t, 2> get2(size_t idx) const {
    const size_t* data = (const size_t*)m_data.data();
    const size_t  bits = m_bits;
    const size_t  pos = bits * idx;
    assert(bits <= 64);
	std::array<size_t, 2> aVals;
    aVals[0] = febitvec::s_get_uint(data, pos, bits);
    aVals[1] = febitvec::s_get_uint(data, pos + bits, bits);
	return aVals;
  }
  static
  size_t fast_get(const byte* data, size_t bits, size_t idx) {
    assert(bits <= 64);
    size_t bitpos = bits * idx;
    return febitvec::s_get_uint((const size_t*)data, bitpos, bits);
  }
  size_t back() const { assert(m_size > 0); return get(m_size-1); }
  size_t operator[](size_t idx) const { return get(idx); }

  std::pair<size_t, size_t>
         equal_range(size_t lo, size_t hi, size_t key) const noexcept;
  size_t lower_bound(size_t lo, size_t hi, size_t key) const noexcept;
  size_t upper_bound(size_t lo, size_t hi, size_t key) const noexcept;

  std::pair<size_t, size_t>
         equal_range(size_t key) const noexcept { return equal_range(0, m_size, key); }
  size_t lower_bound(size_t key) const noexcept { return lower_bound(0, m_size, key); }
  size_t upper_bound(size_t key) const noexcept { return upper_bound(0, m_size, key); }
};

template<class Int>
class ZipIntVector : private UintVecMin0 {
	typedef Int int_value_t;
	Int  m_min_val;
public:
	template<class Int2>
	ZipIntVector(size_t num, Int2 min_val, Int2 max_val) {
		assert(min_val < max_val);
		UintVecMin0::m_bits = 0;
		UintVecMin0::m_mask = 0;
		UintVecMin0::m_size = 0;
		m_min_val = min_val;
#if TERARK_WORD_BITS < 64
		if (sizeof(Int2) > sizeof(size_t))
			resize_with_wire_max_val(num, (ullong)(max_val - min_val));
		else
#endif
			resize_with_wire_max_val(num, size_t(max_val - min_val));
	}
	ZipIntVector() : m_min_val(0) {}
	using UintVecMin0::data;
	using UintVecMin0::size;
	using UintVecMin0::mem_size;
	using UintVecMin0::compute_mem_size;
	using UintVecMin0::clear;
	using UintVecMin0::risk_release_ownership;
	using UintVecMin0::uintbits;
	using UintVecMin0::uintmask;
	using UintVecMin0::prefetch;
	using UintVecMin0::fast_prefetch;
	void swap(ZipIntVector& y) {
		UintVecMin0::swap(y);
		std::swap(m_min_val, y.m_min_val);
	}
	void set(size_t idx, Int val) {
		assert(val >= Int(m_min_val));
		assert(val <= Int(m_min_val + m_mask));
		UintVecMin0::set_wire(idx, val - m_min_val);
	}
	Int operator[](size_t idx) const { return get(idx); }
	Int get(size_t idx) const {
		assert(idx < m_size);
		return Int(m_min_val + UintVecMin0::get(idx));
	}
	void get2(size_t idx, Int aVals[2]) const {
		const byte*  data = m_data.data();
		const size_t bits = m_bits;
		const size_t mask = m_mask;
		Int minVal = m_min_val;
		aVals[0] = fast_get(data, bits, mask, minVal, idx);
		aVals[1] = fast_get(data, bits, mask, minVal, idx+1);
	}
	std::array<Int,2> get2(size_t idx) const {
		const byte*  data = m_data.data();
		const size_t bits = m_bits;
		const size_t mask = m_mask;
		Int minVal = m_min_val;
		std::array<Int,2> aVals;
		aVals[0] = fast_get(data, bits, mask, minVal, idx);
		aVals[1] = fast_get(data, bits, mask, minVal, idx+1);
		return aVals;
	}
	static
	Int fast_get(const byte* data, size_t bits, size_t mask,
				    size_t minVal, size_t idx) {
		return Int(minVal + UintVecMin0::fast_get(data, bits, mask, idx));
	}
	Int min_val() const { return Int(m_min_val); }

	template<class Int2>
	void risk_set_data(byte* Data, size_t num, Int2 min_val, size_t bits) {
		UintVecMin0::risk_set_data(Data, num, bits);
		m_min_val = min_val;
	}

	template<class Int2>
	void build_from(const valvec<Int2>& y) { build_from(y.data(), y.size()); }
	template<class Int2>
	void build_from(const Int2* src, size_t num) {
		m_min_val = UintVecMin0::build_from(src, num);
#if !defined(NDEBUG) && 0
		for(size_t i = 0; i < num; ++i) {
			Int2   x = src[i];
			size_t y = UintVecMin0::get(i);
			assert(Int2(m_min_val + y) == x);
		}
#endif
	}

	std::pair<size_t, size_t>
	equal_range(size_t lo, size_t hi, size_t key) const noexcept {
		assert(lo <= hi);
		assert(hi <= m_size);
		if (terark_likely(m_min_val < key))
			return UintVecMin0::equal_range(lo, hi, key - m_min_val);
		else
			return std::make_pair(lo, lo);
	}
	size_t lower_bound(size_t lo, size_t hi, size_t key) const noexcept {
		assert(lo <= hi);
		assert(hi <= m_size);
		if (terark_likely(m_min_val < key))
			return UintVecMin0::lower_bound(lo, hi, key - m_min_val);
		else
			return lo;
	}
	size_t upper_bound(size_t lo, size_t hi, size_t key) const noexcept {
		assert(lo <= hi);
		assert(hi <= m_size);
		if (terark_likely(m_min_val < key))
			return UintVecMin0::upper_bound(lo, hi, key - m_min_val);
		else
			return lo;
	}

	std::pair<size_t, size_t>
		   equal_range(size_t key) const noexcept { return equal_range(0, m_size, key); }
	size_t lower_bound(size_t key) const noexcept { return lower_bound(0, m_size, key); }
	size_t upper_bound(size_t key) const noexcept { return upper_bound(0, m_size, key); }
};

typedef ZipIntVector<size_t>   UintVector;
typedef ZipIntVector<intptr_t> SintVector;

} // namespace terark
