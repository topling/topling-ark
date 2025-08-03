﻿#pragma once

#include <assert.h>
#include <stddef.h>
#include <string.h>

#include <iterator>
#include <string>
#include <iosfwd>
#include <utility>
#include <algorithm>
#include <functional>
#include <string.h>

#include "config.hpp"
#include "stdtypes.hpp"
#include "util/throw.hpp"
#include "bits_rotate.hpp"

#include <boost/utility/enable_if.hpp>

namespace terark {

#if defined(__GNUC__) || defined(__INTEL_COMPILER) || defined(__clang__)
	#define HSM_FORCE_INLINE __attribute__((always_inline))
#elif defined(_MSC_VER)
	#define HSM_FORCE_INLINE __forceinline
#else
	#define HSM_FORCE_INLINE inline
#endif

// Why name it SP_ALIGN? It is: String Pool/Pointer Align
//
// to disable align, define SP_ALIGN as 0
//    otherwise, it will guess and use the best alignment
#ifdef SP_ALIGN
	#if SP_ALIGN == 0
		#undef  SP_ALIGN
		#define SP_ALIGN 1
	#endif
#elif defined(TERARK_WORD_BITS)
	#if 64 == TERARK_WORD_BITS
	  #define SP_ALIGN 8
	#elif 32 == TERARK_WORD_BITS
	  #define SP_ALIGN 4
	#else
	  #error TERARK_WORD_BITS is invalid
	#endif
#else
	#error TERARK_WORD_BITS is not defined
#endif

#if SP_ALIGN == 4
//	typedef uint32_t align_type;
	typedef unsigned align_type;
	typedef size_t   HSM_HashTp;
#elif SP_ALIGN == 8
//	typedef uint64_t align_type;
	typedef unsigned long long align_type;
	typedef unsigned long long HSM_HashTp;
#elif SP_ALIGN == 1
	typedef unsigned char align_type;
	typedef size_t        HSM_HashTp;
#else
	#error "SP_ALIGN defined but is not 0, 1, 4 or 8"
#endif
#if SP_ALIGN == 4 && UINT_MAX != 0xFFFFFFFF
	#error "sizeof(unsigned) must be 4"
#elif SP_ALIGN == 8 && ULLONG_MAX != 0xFFFFFFFFFFFFFFFFull
	#error "sizeof(unsigned long long) must be 8"
#endif
BOOST_STATIC_ASSERT(sizeof(align_type) == SP_ALIGN);

#if SP_ALIGN == 1
	#define LOAD_OFFSET(x)  size_t(x)
	#define SAVE_OFFSET(x)  LinkTp(x)
	#define IF_SP_ALIGN(Then, Else) Else
#else
	// on 64 bit system, make it to support larger strpool, up to 8*4G = 32G
	#define LOAD_OFFSET(x)  (size_t(x) * SP_ALIGN)
	#define SAVE_OFFSET(x)  LinkTp((x) / SP_ALIGN)
	#define IF_SP_ALIGN(Then, Else) Then
#endif

inline size_t terark_fstrlen(const char* s) { return strlen(s); }
inline size_t terark_fstrlen(const uint16_t* s) {
	size_t n = 0;
	while (s[n]) ++n;
	return n;
}

inline const char*
terark_fstrchr(const char* str, size_t len, unsigned char ch) {
	return (char*)memchr(str, ch, len);
}

inline const uint16_t*
terark_fstrchr(const uint16_t* str, size_t len, uint16_t ch) {
	for (; len; --len, ++str) {
		if (ch == *str)
			return str;
	}
	return NULL;
}

#ifdef _MSC_VER
TERARK_DLL_EXPORT
char*
terark_fstrstr(const char* haystack, size_t haystack_len
		   , const char* needle  , size_t needle_len);
#else
inline char*
terark_fstrstr(const char* haystack, size_t haystack_len
		   , const char* needle  , size_t needle_len)
{
	return (char*)memmem(haystack, haystack_len, needle, needle_len);
}
#endif

TERARK_DLL_EXPORT
uint16_t*
terark_fstrstr(const uint16_t* haystack, size_t haystack_len
		   , const uint16_t* needle  , size_t needle_len);

template<class T>
inline
size_t unmatchPos(const T* x, const T* y, size_t begPos, size_t endPos) {
	size_t i = begPos;
	while (i < endPos && x[i] == y[i]) ++i;
	return i;
}

template<class Str>
inline
size_t commonPrefixLen(const Str& x, const Str& y) {
	size_t xn = x.size(), yn = y.size();
	size_t minlen = xn < yn ? xn : yn;
	return unmatchPos(x.data(), y.data(), 0, minlen);
}

template<class Char>
struct terark_get_uchar_type;

template<>struct terark_get_uchar_type<char>{typedef unsigned char type;};
template<>struct terark_get_uchar_type<uint16_t>{typedef uint16_t type;};

// for SFINAE
#define DOT_STAR_S(s) (int)((s).size()), (s).data()

// Fast String: shallow copy, simple, just has char* and length
// May be short name of: Febird String
template<class Char>
struct TERARK_DLL_EXPORT basic_fstring {
//	BOOST_STATIC_ASSERT(sizeof(Char) <= 2);
	typedef std::basic_string<Char> std_string;
	const Char* p;
	ptrdiff_t   n;

	static constexpr size_t npos = size_t(-1);

	basic_fstring() : p(NULL), n(0) {}
	basic_fstring(const std_string& x) : p(x.c_str()), n(x.size()) {}
#ifdef NDEBUG // let compiler compute strlen(string literal) at compilation time
	basic_fstring(const Char* x) : p(x), n(terark_fstrlen(x)) {}
#else
	basic_fstring(const Char* x) { assert(NULL != x); p = x; n = terark_fstrlen(x); }
#endif
	basic_fstring(const Char* x, ptrdiff_t   l) : p(x), n(l  ) { assert(l >= 0); }
	basic_fstring(const Char* x, const Char* y) : p(x), n(y-x) { assert(y >= x); }

#define fstring_enable_if_same_size(C) typename boost::enable_if_c<sizeof(C)==sizeof(Char)>::type* = NULL
	template<class C> basic_fstring(const C* x, fstring_enable_if_same_size(C)) { assert(NULL != x); p = (const Char*)x; n = terark_fstrlen((const Char*)x); }
	template<class C> basic_fstring(const C* x, ptrdiff_t l, fstring_enable_if_same_size(C)) : p((const Char*)x), n(l  ) { assert(l >= 0); }
	template<class C> basic_fstring(const C* x, const C*  y, fstring_enable_if_same_size(C)) : p((const Char*)x), n(y-x) { assert(y >= x); }
	template<class C> basic_fstring(std::pair<const C*, const C*> x, fstring_enable_if_same_size(C)) : p((const Char*)x.first), n(x.second - x.first) { assert(x.first <= x.second); }
	template<class C> basic_fstring(std::pair<C*, C*> x, fstring_enable_if_same_size(C)) : p((const Char*)x.first), n(x.second - x.first) { assert(x.first <= x.second); }
#undef fstring_enable_if_same_size

	basic_fstring(const std::pair<Char*, Char*>& rng) : p(rng.first), n(rng.second - rng.first) { assert(n >= 0); }
	basic_fstring(const std::pair<const Char*, const Char*>& rng) : p(rng.first), n(rng.second - rng.first) { assert(n >= 0); }

	template<class CharVec>
	basic_fstring(const CharVec& chvec
#if defined(_MSC_VER) && _MSC_VER < 1500
		, typename CharVec::value_type* = nullptr
		, typename CharVec::iterator* = nullptr
		, typename CharVec::const_iterator* = nullptr
#elif defined(_MSC_VER)
		, decltype(((const CharVec*)(nullptr))->data()) = nullptr
#else
		, decltype(chvec.data()) = nullptr
#endif
	) {
		BOOST_STATIC_ASSERT(sizeof(*chvec.data()) == sizeof(Char));
		p = (const Char*)&*chvec.data();
		n = chvec.size();
	}

	const std::pair<const Char*, const Char*> range() const { return std::make_pair(p, p+n); }

	template<class AnyStringType>
	operator decltype(AnyStringType((const Char*)0, 1))
	() const { return AnyStringType(p, n); }

	typedef ptrdiff_t difference_type;
	typedef    size_t       size_type;
	typedef const Char     value_type;
	typedef const Char &reference, &const_reference;
	typedef const Char *iterator, *const_iterator;
	typedef std::reverse_iterator<iterator> reverse_iterator, const_reverse_iterator;
	typedef typename terark_get_uchar_type<Char>::type uc_t;

	iterator  begin() const { return p; }
	iterator cbegin() const { return p; }
	iterator    end() const { return p + n; }
	iterator   cend() const { return p + n; }
	reverse_iterator  rbegin() const { return reverse_iterator(p + n); }
	reverse_iterator crbegin() const { return reverse_iterator(p + n); }
	reverse_iterator    rend() const { return reverse_iterator(p); }
	reverse_iterator   crend() const { return reverse_iterator(p); }

	uc_t ende(ptrdiff_t off) const {
		assert(off <= n);
		assert(off >= 1);
		return p[n-off];
	}

	std_string    str() const { assert(0==n || (p && n) ); return std_string(p, n); }
	const Char* c_str() const { assert(p && '\0' == p[n]); return p; }
	const Char*  data() const { return p; }
	const uc_t* udata() const { return (const uc_t*)p; }
	size_t       size() const { return n; }
	int          ilen() const { return (int)n; } // for printf: "%.*s"
	uc_t operator[](ptrdiff_t i)const{assert(i>=0);assert(i<n);assert(p);return p[i];}
	uc_t        uch(ptrdiff_t i)const{assert(i>=0);assert(i<n);assert(p);return p[i];}

	size_t length() const { return n; }

	bool empty() const { return 0 == n; }
	basic_fstring& chomp();
	basic_fstring& trim();

	basic_fstring substr(size_t pos, size_t len) const {
		assert(pos <= size());
		assert(len <= size());
	//	if (pos > size()) { // similar with std::basic_string::substr
	//		THROW_STD(out_of_range, "size()=%zd pos=%zd", size(), pos);
	//	}
		if (pos + len > size()) len = size() - pos;
	   	return basic_fstring(p+pos, len);
   	}
	basic_fstring substr(size_t pos) const {
		assert(pos <= size());
	//	if (pos > size()) { // similar with std::basic_string::substr
	//		THROW_STD(out_of_range, "size()=%zd pos=%zd", size(), pos);
	//	}
	   	return basic_fstring(p+pos, n-pos);
   	}
	basic_fstring substrBegEnd(size_t Beg, size_t End) const {
		assert(Beg <= End);
		assert(End <= size());
	//	if (End > size()) { // similar with std::basic_string::substr
	//		THROW_STD(out_of_range, "size()=%zd End=%zd", size(), End);
	//	}
	   	return basic_fstring(p+Beg, End-Beg);
	}
    basic_fstring prefix(size_t len) const {
        assert(len <= size_t(n));
        return basic_fstring(p, len);
    }
    basic_fstring suffix(size_t len) const {
        assert(len <= size_t(n));
        return basic_fstring(p+n-len, len);
    }
    basic_fstring notail(size_t len) const {
        assert(len <= size_t(n));
        return basic_fstring(p, n-len);
    }
	std::string hex() const noexcept;

	bool matchAt(ptrdiff_t pos, Char ch) const {
		assert(pos >= 0);
		assert(pos <= n);
		return pos < n && p[pos] == ch;
	}
	bool matchAt(ptrdiff_t pos, basic_fstring needle) const {
		assert(pos >= 0);
		assert(pos <= n);
		if (pos + needle.n > n) return false;
		return memcmp(p + pos, needle.p, sizeof(Char) * needle.size()) == 0;
	}

	const Char* strchr(uc_t ch) const { return terark_fstrchr(p, n, ch); }
	const Char* strstr(basic_fstring needle) const {
		assert(needle.n > 0);
		return this->strstr(0, needle);
	}
	const Char* strstr(ptrdiff_t pos, basic_fstring needle) const {
		assert(pos >= 0);
		assert(pos <= n);
		assert(needle.n > 0);
		if (pos + needle.n > n) return NULL;
		return terark_fstrstr(p + pos, n - pos, needle.p, needle.n);
	}

	size_t find(ptrdiff_t pos, basic_fstring needle) const {
		assert(pos >= 0);
		assert(pos <= n);
		assert(needle.n > 0);
		if (pos + needle.n > n) return npos;
		const Char* hit = terark_fstrstr(p + pos, n - pos, needle.p, needle.n);
		return hit ? hit - p : npos;
	}
	size_t find(basic_fstring needle) const {
		return find(0, needle);
	}

	bool startsWith(basic_fstring x) const {
		if (x.n > n) return false;
		return memcmp(p, x.p, sizeof(Char)*x.n) == 0;
	}
	bool endsWith(basic_fstring x) const {
		if (x.n > n) return false;
		return memcmp(p+n - x.n, x.p, sizeof(Char)*x.n) == 0;
	}

	size_t commonPrefixLen(basic_fstring y) const {
		size_t minlen = n < y.n ? n : y.n;
		for (size_t i = 0; i < minlen; ++i)
			if (p[i] != y.p[i]) return i;
		return minlen;
	}

	size_t commonPrefixLen(const Char* str, ptrdiff_t len) const {
		size_t minlen = n < len ? n : len;
		for (size_t i = 0; i < minlen; ++i)
			if (p[i] != str[i]) return i;
		return minlen;
	}

	///@{ to compatible with other similar class such as Slice ...
	bool starts_with(basic_fstring x) const { return startsWith(x); }
	bool ends_with(basic_fstring x) const { return endsWith(x); }
	size_t difference_offset(basic_fstring x) { return commonPrefixLen(x); }
	inline int compare(basic_fstring y) const noexcept {
		int cmp = memcmp(p, y.p, std::min(n, y.n));
		if (cmp) return cmp;
		else return n < y.n ? -1 :
					n > y.n ? +1 : 0;
	}
	//@}

	template<class StrClass>
	void append_to(StrClass* str) const { str->append(p, n); }

	template<class Vec>
	size_t split(const Char delim, Vec* F, size_t max_fields = ~size_t(0)) const {
		F->resize(0);
		if (' ' == delim) {
		   	// same as awk, skip first blank field, and skip dup blanks
			const Char *col = p, *End = p + n;
			while (col < End && isspace((unsigned char)(*col)))
				 ++col; // skip first blank field
			while (col < End && F->size()+1 < max_fields) {
				const Char* next = col;
				while (next < End && !isspace((unsigned char)(*next))) ++next;
				F->push_back(typename Vec::value_type(col, next));
				while (next < End &&  isspace((unsigned char)(*next))) ++next; // skip blanks
				col = next;
			}
			if (col < End)
				F->push_back(typename Vec::value_type(col, End));
		} else {
			const Char *col = p, *End = p + n;
			while (col <= End && F->size()+1 < max_fields) {
				const Char* next = col;
				while (next < End && delim != *next) ++next;
				F->push_back(typename Vec::value_type(col, next));
				col = next + 1;
			}
			if (col <= End)
				F->push_back(typename Vec::value_type(col, End));
		}
		return F->size();
	}

	/// split into fields
	template<class Vec>
	size_t split(const Char* delims, Vec* F, size_t max_fields = ~size_t(0)) {
		assert(n >= 0);
		size_t dlen = terark_fstrlen(delims);
		if (0 == dlen) // empty delims redirect to blank delim
			return split(' ', F, max_fields);
		if (1 == dlen)
			return split(delims[0], F, max_fields);
		F->resize(0);
		const Char *col = p, *End = p + n;
		while (col <= End && F->size()+1 < max_fields) {
			const Char* next = terark_fstrstr(col, End-col, delims, dlen);
			if (NULL == next) next = End;
			F->push_back(typename Vec::value_type(col, next));
			col = next + dlen;
		}
		if (col <= End)
			F->push_back(typename Vec::value_type(col, End));
		return F->size();
	}

	template<class Func>
	size_t split_f3(const Char delim, Func F, size_t max_fields = ~size_t(0)) const {
		size_t nFields = 0;
		if (' ' == delim) {
			// same as awk, skip first blank field, and skip dup blanks
			const Char *col = p, *End = p + n;
			while (col < End && isspace((unsigned char)(*col)))
				++col; // skip first blank field
			while (col < End && nFields + 1 < max_fields) {
				const Char* next = col;
				while (next < End && !isspace((unsigned char)(*next))) ++next;
				F(col, next, nFields);
				while (next < End &&  isspace((unsigned char)(*next))) ++next; // skip blanks
				col = next;
				nFields++;
			}
			if (col < End) {
				F(col, End, nFields);
				nFields++;
			}
		}
		else {
			const Char *col = p, *End = p + n;
			while (col <= End && nFields + 1 < max_fields) {
				const Char* next = col;
				while (next < End && delim != *next) ++next;
				F(col, next, nFields);
				col = next + 1;
				nFields++;
			}
			if (col <= End) {
				F(col, End, nFields);
				nFields++;
			}
		}
		return nFields;
	}

	template<class Func>
	size_t split_f3(const Char* delims, Func F, size_t max_fields = ~size_t(0)) {
		size_t dlen = terark_fstrlen(delims);
		if (0 == dlen) // empty delims redirect to blank delim
			return split_f3<Func>(' ', F, max_fields);
		if (1 == dlen)
			return split_f3<Func>(delims[0], F, max_fields);
		size_t nFields = 0;
		const Char *col = p, *End = p + n;
		while (col <= End && nFields + 1 < max_fields) {
			const Char* next = terark_fstrstr(col, End - col, delims, dlen);
			if (NULL == next) next = End;
			F(col, next, nFields);
			col = next + dlen;
			nFields++;
		}
		if (col <= End) {
			F(col, End, nFields);
			nFields++;
		}
		return nFields;
	}

private:
	template<class Func2>
	struct Func2_to_Func3 {
		const Func2* f2;
		Func2_to_Func3(const Func2& f) : f2(&f) {}
		void operator()(const Char* beg, const Char* end, size_t/*nFields*/)
		const { (*f2)(beg, end); }
	};
public:
	template<class Delim, class Func>
	size_t split_f2(Delim delim, Func F, size_t max_fields = ~size_t(0)) {
		return split_f3(delim, Func2_to_Func3<Func>(F), max_fields);
	}
};

template<class DataIO, class Char>
void DataIO_saveObject(DataIO& dio, basic_fstring<Char> s) {
	dio << typename DataIO::my_var_uint64_t(s.n);
	dio.ensureWrite(s.p, sizeof(Char) * s.n);
}

typedef basic_fstring<char> fstring;
typedef basic_fstring<uint16_t> fstring16;

template<class Char> struct char_to_fstring;
template<> struct char_to_fstring<char> { typedef fstring type; };
template<> struct char_to_fstring<unsigned char> { typedef fstring type; };
template<> struct char_to_fstring<uint16_t> { typedef fstring16 type; };

TERARK_DLL_EXPORT
std::string operator+(fstring x, fstring y);
inline std::string operator+(fstring x, const char* y) {return x+fstring(y);}
inline std::string operator+(const char* x, fstring y) {return fstring(x)+y;}
#if defined(__GXX_EXPERIMENTAL_CXX0X__) || __cplusplus >= 201103L || defined(_MSC_VER) && _MSC_VER >= 1400
inline std::string operator+(std::string&& x, fstring y) { return x.append(   y.p, y.n); }
inline std::string operator+(fstring x, std::string&& y) { return y.insert(0, x.p, x.n); }
#endif

#if defined(__GNUC__) && __GNUC__*1000 + __GNUC_MINOR__ >= 11001
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wstringop-overread"
#endif
inline bool operator==(fstring x, fstring y) {
    if (x.n != y.n)
        return false;
    return memcmp(x.p, y.p, x.n) == 0;
}
inline bool operator!=(fstring x, fstring y) { return !(x == y); }
inline bool operator< (fstring x, fstring y) {
	using namespace std;
    int cmp = memcmp(x.p, y.p, min(x.n, y.n));
    if (cmp)
        return cmp < 0;
    else
        return x.n < y.n;
}
inline bool operator> (fstring x, fstring y) { return  (y < x); }
inline bool operator<=(fstring x, fstring y) { return !(y < x); }
inline bool operator>=(fstring x, fstring y) { return !(x < y); }

#if defined(__GNUC__) && __GNUC__*1000 + __GNUC_MINOR__ >= 11001
#  pragma GCC diagnostic pop
#endif

TERARK_DLL_EXPORT std::ostream& operator<<(std::ostream& os, fstring s);

// fstring16
TERARK_DLL_EXPORT bool operator==(fstring16 x, fstring16 y);
TERARK_DLL_EXPORT bool operator!=(fstring16 x, fstring16 y);

TERARK_DLL_EXPORT bool operator<(fstring16 x, fstring16 y);
TERARK_DLL_EXPORT bool operator>(fstring16 x, fstring16 y);

TERARK_DLL_EXPORT bool operator<=(fstring16 x, fstring16 y);
TERARK_DLL_EXPORT bool operator>=(fstring16 x, fstring16 y);

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
struct fstring_func {
	// 3-way compare
	class prefix_compare3 {
		ptrdiff_t plen;
	public:
		int operator()(fstring x, fstring y) const {
			using namespace std;
			const ptrdiff_t lmin = min(x.n, y.n);
			const ptrdiff_t clen = min(lmin, plen);
			for (ptrdiff_t i = 0; i < clen; ++i)
				if (x.p[i] != y.p[i]) // char diff doesn't exceed INT_MAX
					return (unsigned char)x.p[i] - (unsigned char)y.p[i];
			if (plen < lmin)
				return 0; // all prefix are same
			else
				return int(x.n - y.n); // length diff couldn't exceed INT_MAX
		}
		explicit prefix_compare3(ptrdiff_t prelen) : plen(prelen) {}
	};

	// 3-way compare
	class compare3 {
	public:
		int operator()(fstring x, fstring y) const {
			using namespace std;
			int ret = memcmp(x.p, y.p, min(x.n, y.n));
			if (ret != 0)
				return ret;
			return int(x.n - y.n); // length diff couldn't exceed INT_MAX
		}
	};

	struct less_unalign {
		bool operator()(const fstring& x, const fstring& y) const {
			int ret = memcmp(x.p, y.p, x.n < y.n ? x.n : y.n);
			if (ret != 0)
				return ret < 0;
			else
				return x.n < y.n;
		}
	};
	struct hash_unalign {
		HSM_FORCE_INLINE
		HSM_HashTp operator()(const fstring k) const {
			HSM_HashTp h = 2134173 + k.n * 31;
			for (ptrdiff_t i = 0; i < k.n; ++i)
				h = BitsRotateLeft(h, 5) + h + k.p[i];
			return h;
		}
	};
	struct equal_unalign {
		bool operator()(const fstring x, const fstring y) const {
			return x.n == y.n && memcmp(x.p, y.p, x.n) == 0;
		}
	};

	static size_t align_to(size_t x) {
		return (x + SP_ALIGN-1) & ~(intptr_t)(SP_ALIGN-1);
	}
#if SP_ALIGN != 1
	struct less_align {
		HSM_FORCE_INLINE
		bool operator()(const fstring& x, const fstring& y) const {
			ptrdiff_t n = x.n < y.n ? x.n : y.n;
			ptrdiff_t c = n - (SP_ALIGN-1);
			ptrdiff_t i = 0;
			for (; i < c; i += SP_ALIGN)
				if (*reinterpret_cast<const align_type*>(x.p + i) !=
					*reinterpret_cast<const align_type*>(y.p + i))
						break;
			for (; i < n; ++i)
				if (x.p[i] != y.p[i])
					return (unsigned char)x.p[i] < (unsigned char)y.p[i];
			return x.n < y.n;
		}
	};
	struct Less {
		HSM_FORCE_INLINE
		bool operator()(const fstring& x, const fstring& y) const {
			if (((intptr_t(x.p) | intptr_t(y.p)) & (SP_ALIGN-1)) == 0)
				return less_align()(x, y);
			else
				return less_unalign()(x, y);
		}
	};
	struct hash_align {
		HSM_FORCE_INLINE
		HSM_HashTp operator()(const fstring k) const {
			BOOST_STATIC_ASSERT(SP_ALIGN <= sizeof(HSM_HashTp));
			HSM_HashTp h = 2134173 + k.n * 31;
			ptrdiff_t c = k.n - (SP_ALIGN-1);
			ptrdiff_t i = 0;
			for (; i < c; i += SP_ALIGN)
				h = BitsRotateLeft(h, 5) + h + *reinterpret_cast<const align_type*>(k.p + i);
			for (; i < k.n; ++i)
				h = BitsRotateLeft(h, 5) + h + k.p[i];
			return h;
		}
	};

// make it easier to enable or disable X86 judgment
#if 1 && ( \
	defined(__i386__) || defined(__i386) || defined(_M_IX86) || \
	defined(__X86__) || defined(_X86_) || \
	defined(__THW_INTEL__) || defined(__I86__) || \
	defined(__amd64__) || defined(__amd64) || defined(__x86_64__) || defined(__x86_64) || defined(_M_X64) \
	)
	// don't care pointer align, overhead of x86 align fault is slightly enough
	typedef hash_align hash;
#else
	struct hash { // align or not align
		HSM_FORCE_INLINE
		HSM_HashTp operator()(const fstring k) const {
			HSM_HashTp h = 2134173 + k.n * 31;
			ptrdiff_t c = k.n - (SP_ALIGN-1);
			ptrdiff_t i = 0;
			if ((intptr_t(k.p) & (SP_ALIGN-1)) == 0) { // aligned
				for (; i < c; i += SP_ALIGN)
					h = BitsRotateLeft(h, 5) + h + *reinterpret_cast<const align_type*>(k.p + i);
			}
			else { // not aligned
				for (; i < c; i += SP_ALIGN) {
					align_type word;
					// compiler could generate unaligned load instruction
					// for fixed size(SP_ALIGN) object, word may be a register
					memcpy(&word, k.p + i, SP_ALIGN);
					h = BitsRotateLeft(h, 5) + h  + word;
				}
			}
			for (; i < k.n; ++i)
				h = BitsRotateLeft(h, 5) + h + k.p[i];
			return h;
		}
	};
#endif // X86
	struct equal_align {
		HSM_FORCE_INLINE
		bool operator()(const fstring x, const fstring y) const {
			if (x.n != y.n)
				return false;
			ptrdiff_t c = x.n - (SP_ALIGN-1);
			ptrdiff_t i = 0;
			for (; i < c; i += SP_ALIGN)
				if (*reinterpret_cast<const align_type*>(x.p + i) !=
					*reinterpret_cast<const align_type*>(y.p + i))
						return false;
			for (; i < x.n; ++i)
				if (x.p[i] != y.p[i])
					return false;
			return true;
		}
	};
	struct equal { // align or not align
		HSM_FORCE_INLINE
		bool operator()(const fstring x, const fstring y) const {
			if (x.n != y.n)
				return false;
			if (((intptr_t(x.p) | intptr_t(y.p)) & (SP_ALIGN-1)) == 0) {
				ptrdiff_t c = x.n - (SP_ALIGN-1);
				ptrdiff_t i = 0;
				for (; i < c; i += SP_ALIGN)
					if (*reinterpret_cast<const align_type*>(x.p + i) !=
						*reinterpret_cast<const align_type*>(y.p + i))
							return false;
				for (; i < x.n; ++i)
					if (x.p[i] != y.p[i])
						return false;
				return true;
			} else
				return memcmp(x.p, y.p, x.n) == 0;
		}
	};
#else
	typedef less_unalign less_align;
	typedef hash_unalign hash_align;
	typedef equal_unalign equal_align;
	typedef less_unalign Less;
	typedef hash_unalign hash;
	typedef equal_unalign equal;
#endif // SP_ALIGN
};

TERARK_DLL_EXPORT extern unsigned char gtab_ascii_tolower[256];

TERARK_DLL_EXPORT bool parseBooleanRelaxed(const char* str, bool Default) noexcept;
TERARK_DLL_EXPORT bool getEnvBool(const char* envName, bool Default = false) noexcept;
TERARK_DLL_EXPORT long getEnvLong(const char* envName, long Default = false) noexcept;
TERARK_DLL_EXPORT double getEnvDouble(const char* envName, double Default) noexcept;
TERARK_DLL_EXPORT const char* getEnvStr(const char* envName, const char* Default = nullptr);

TERARK_DLL_EXPORT unsigned long long ParseSizeXiB(const char* str) noexcept;
TERARK_DLL_EXPORT unsigned long long ParseSizeXiB(fstring str) noexcept;
TERARK_DLL_EXPORT unsigned long long ParseSizeXiB(const char* str, const char* Default) noexcept;
TERARK_DLL_EXPORT unsigned long long ParseSizeXiB(const char* str, unsigned long long) noexcept;
TERARK_DLL_EXPORT unsigned long long ScaleSizeXiB(double val, char scale) noexcept;

/// default quote = '\0' indicate do not escape quote
TERARK_DLL_EXPORT void escape_append(fstring, std::string*, char quote = '\0');
TERARK_DLL_EXPORT std::string escape(fstring, char quote = '\0');

TERARK_DLL_EXPORT void string_resize_no_touch_memory(std::string*, size_t);

} // namespace terark

namespace std {
	template<> struct hash<terark::fstring> : terark::fstring_func::hash {};
}
