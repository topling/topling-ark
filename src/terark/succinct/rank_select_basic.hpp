#pragma once

#include <terark/bitmap.hpp>
#include <terark/util/throw.hpp>
#include <limits>

#ifdef __BMI2__
#   include "rank_select_inline_bmi2.hpp"
#else
#   include "rank_select_inline_slow.hpp"
#endif

#if defined(__BMI2__) && TERARK_WORD_BITS == 64
// plain extract bits may be faster than _bextr_u64 in some CPU,
// but _bextr_u64 is always faster when extract const bit pos and width
// rank512 is mostly used to extract const bit pos and width
// #   define rank512(bm64, i) TERARK_GET_BITS_64(bm64, i, 9)
#   define rank512(bm64, i) _bextr_u64(bm64, (i-1)*9, 9)
#else
#   define rank512(bm64, i) ((bm64 >> (i-1)*9) & 511)
#endif

#define rank_select_check_overflow(SIZE, OP, TYPE)                   \
    do {                                                             \
        if ((SIZE) OP size_t(std::numeric_limits<uint32_t>::max()))  \
            TERARK_DIE(#TYPE" overflow , size = %zd", size_t(SIZE)); \
    } while (false)

namespace terark {

template<size_t iLineBits>
struct RankSelectConstants {
    static_assert((iLineBits & (iLineBits - 1)) == 0, "iLineBits must be power of 2");
    static const size_t LineBits = iLineBits;
    static const size_t LineShift = StaticUintBits<LineBits>::value - 1;
    static const size_t LineWords = LineBits / WordBits;

    static size_t BitsToLines(size_t nbits)
      { return (nbits + LineBits - 1) / LineBits; }
};

} // namespace terark
