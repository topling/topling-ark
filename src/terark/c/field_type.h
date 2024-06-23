#ifndef __terark_c_field_type_h__
#define __terark_c_field_type_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
# pragma warning(disable: 4127)
#endif

#include "config.h"
#include <stdarg.h>

#ifdef __cplusplus

#  include <string> // for std::string

extern "C" {
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
typedef unsigned __int64 terark_uint64_t;
typedef __int64 terark_int64_t;
#else
typedef unsigned long long terark_uint64_t;
typedef long long terark_int64_t;
#endif

struct terark_fixed_blob { unsigned char data[1]; };

enum field_type_t {
	tev_char,
	tev_byte,
	tev_int16,
	tev_uint16,
	tev_int32,
	tev_uint32,
	tev_int64,
	tev_uint64,
	tev_float,
	tev_double,
#if defined(TERARK_C_LONG_DOUBLE_SIZE)
	tev_ldouble,
#endif
	tev_int,
	tev_uint,
	tev_long,
	tev_ulong,
	tev_ptr,
	tev_app,   ///< application defined type
	tev_blob,  ///< blob
	tev_cstr,
	tev_inline_cstr,
	tev_std_string, ///< for c++ std::string, only used in c++
	tev_std_wstring, ///< for c++ std::wstring, only used in c++
	tev_all_count__
#define	tev_type_count__  tev_int
};
typedef enum field_type_t field_type_t;

TERARK_DLL_EXPORT
int
field_type_size(field_type_t t);

TERARK_DLL_EXPORT
const char*
field_type_name_str(field_type_t t);

TERARK_DLL_EXPORT
field_type_t
internal_field_type(field_type_t field_type);

#ifdef __cplusplus
} // extern "C"

struct terark_inline_cstr_tag{};

inline field_type_t terark_get_field_type(const char*)           { return tev_char; }
inline field_type_t terark_get_field_type(const signed char*)    { return tev_char; }
inline field_type_t terark_get_field_type(const unsigned char*)  { return tev_byte; }

inline field_type_t terark_get_field_type(const short*)          { return tev_int16; }
inline field_type_t terark_get_field_type(const unsigned short*) { return tev_uint16; }

inline field_type_t terark_get_field_type(const int*)            { return tev_int32; }
inline field_type_t terark_get_field_type(const unsigned int*)   { return tev_uint32; }
inline field_type_t terark_get_field_type(const long*)           { return sizeof(long) == 4 ? tev_uint32 : tev_uint64; }
inline field_type_t terark_get_field_type(const unsigned long*)  { return sizeof(unsigned long) == 4 ? tev_uint32 : tev_uint64; }
inline field_type_t terark_get_field_type(const terark_uint64_t*){ return tev_uint64; }
inline field_type_t terark_get_field_type(const terark_int64_t*) { return tev_int64; }
inline field_type_t terark_get_field_type(const float*)          { return tev_float; }
inline field_type_t terark_get_field_type(const double*)         { return tev_double; }

#if defined(TERARK_C_LONG_DOUBLE_SIZE)
inline field_type_t terark_get_field_type(const long double*)    { return tev_ldouble; }
#endif

inline field_type_t terark_get_field_type(const terark_fixed_blob*)    { return tev_app; }

inline field_type_t terark_get_field_type(const std::string*)    { return tev_std_string; }
inline field_type_t terark_get_field_type(const std::wstring*)    { return tev_std_wstring; }

template<class UnknowType>
inline field_type_t terark_get_field_type(const UnknowType*)    { return tev_app; }

template<class BePointed>
inline field_type_t terark_get_field_type(const BePointed**)    { return tev_ptr; }

template<class BePointed>
inline field_type_t terark_get_field_type(const BePointed*const*)    { return tev_ptr; }

inline field_type_t terark_get_field_type(const void**)    { return tev_ptr; }
inline field_type_t terark_get_field_type(const void*const*)    { return tev_ptr; }

inline field_type_t terark_get_field_type(const terark_inline_cstr_tag*)    { return tev_inline_cstr; }

// template<class T>inline field_type_t terark_get_field_type(const T*const *){ return tev_ptr; }
// template<class T>inline field_type_t terark_get_field_type(const T**){ return tev_ptr; }
// template<class T>inline field_type_t terark_get_field_type(T*const *){ return tev_ptr; }

#endif // __cplusplus

#endif // __terark_c_field_type_h__


