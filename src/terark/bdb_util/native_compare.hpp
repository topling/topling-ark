#ifndef __terark_bdb_util_native_compare_hpp__
#define __terark_bdb_util_native_compare_hpp__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

#if defined(_WIN32) || defined(_WIN64)
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif
#endif

#include <db.h>
#include <assert.h>
#include <terark/config.hpp>

#ifdef __cplusplus
#include <string>
//#include <terark/const_string.hpp>
#include <terark/io/var_int.hpp>
#include <db_cxx.h>
#include <boost/tuple/tuple.hpp>

template<class StrT>
class basic_dbt_string
{
	StrT m_str;

public:
	typedef StrT str_t;
	typedef typename StrT::value_type value_type;

	basic_dbt_string() {}
	basic_dbt_string(const StrT& x) : m_str(x) {}
	basic_dbt_string(const value_type* first, const value_type* last)
		: m_str(first, last) {}
	basic_dbt_string(const value_type* first, size_t len)
		: m_str(first, len) {}
	basic_dbt_string(const value_type* first)
		: m_str(first) {}

	StrT* operator->() { return &m_str; }
	const StrT* operator->() const { return &m_str; }
	StrT& str() { return m_str; }
	const StrT& str() const { return m_str; }

	template<class Input>
	friend void DataIO_loadObject(Input& in, basic_dbt_string& x)
	{
		size_t len = in.getStream()->remain() / sizeof(value_type);
		x.m_str.resize(len);
		in.load(&*x.m_str.begin(), len);
		assert(in.getStream()->eof());
	}
	template<class Output>
	friend void DataIO_saveObject(Output& out, const basic_dbt_string& x)
	{
		out.save(x.m_str.data(), x.m_str.size());
	}
};
typedef basic_dbt_string<std::string> dbt_string;
typedef basic_dbt_string<std::wstring> dbt_wstring;

extern "C" {
#endif

TERARK_DLL_EXPORT size_t bdb_fixed_key_prefix(DB *db, const DBT *dbt1, const DBT *dbt2);
TERARK_DLL_EXPORT size_t bdb_common_byte_prefix(DB *db, const DBT *dbt1, const DBT *dbt2);

TERARK_DLL_EXPORT int bdb_compare_uint32(DB *db, const DBT *dbt1, const DBT *dbt2);
TERARK_DLL_EXPORT int bdb_compare_uint64(DB *db, const DBT *dbt1, const DBT *dbt2);
TERARK_DLL_EXPORT int bdb_compare_int32(DB *db, const DBT *dbt1, const DBT *dbt2);
TERARK_DLL_EXPORT int bdb_compare_int64(DB *db, const DBT *dbt1, const DBT *dbt2);

TERARK_DLL_EXPORT int bdb_compare_float(DB *db, const DBT *dbt1, const DBT *dbt2);
TERARK_DLL_EXPORT int bdb_compare_double(DB *db, const DBT *dbt1, const DBT *dbt2);
TERARK_DLL_EXPORT int bdb_compare_long_double(DB *db, const DBT *dbt1, const DBT *dbt2);

TERARK_DLL_EXPORT int bdb_strcmp_nocase(DB *db, const DBT *dbt1, const DBT *dbt2);
TERARK_DLL_EXPORT int bdb_compare_string(DB *db, const DBT *dbt1, const DBT *dbt2);

#ifdef __cplusplus

//! HAS NO string length saved
// DataFormat: var_int + string_content
TERARK_DLL_EXPORT int bdb_cmp_var_uint32_str(DB *db, const DBT *dbt1, const DBT *dbt2);

//! HAS string length saved
// DataFormat: var_int_strlen + string_content + var_int
TERARK_DLL_EXPORT int bdb_cmp_str_var_uint32(DB *db, const DBT *dbt1, const DBT *dbt2);

TERARK_DLL_EXPORT int bdb_mash_strcmp_case(DB *db, const DBT *dbt1, const DBT *dbt2);
TERARK_DLL_EXPORT int bdb_mash_strcmp_nocase(DB *db, const DBT *dbt1, const DBT *dbt2);
TERARK_DLL_EXPORT int bdb_locale_mash_strcmp_case(DB *db, const DBT *dbt1, const DBT *dbt2);
TERARK_DLL_EXPORT int bdb_locale_mash_strcmp_nocase(DB *db, const DBT *dbt1, const DBT *dbt2);

TERARK_DLL_EXPORT int bdb_locale_strcmp_case(DB *db, const DBT *dbt1, const DBT *dbt2);
TERARK_DLL_EXPORT int bdb_locale_strcmp_nocase(DB *db, const DBT *dbt1, const DBT *dbt2);

TERARK_DLL_EXPORT int bdb_compare_var_uint32(DB *db, const DBT *dbt1, const DBT *dbt2);
TERARK_DLL_EXPORT int bdb_compare_var_uint64(DB *db, const DBT *dbt1, const DBT *dbt2);
TERARK_DLL_EXPORT int bdb_compare_var_int32(DB *db, const DBT *dbt1, const DBT *dbt2);
TERARK_DLL_EXPORT int bdb_compare_var_int64(DB *db, const DBT *dbt1, const DBT *dbt2);

TERARK_DLL_EXPORT int bdb_compare_var_uint32_n(DB *db, const DBT *dbt1, const DBT *dbt2, int n);
TERARK_DLL_EXPORT int bdb_compare_var_uint64_n(DB *db, const DBT *dbt1, const DBT *dbt2, int n);
TERARK_DLL_EXPORT int bdb_compare_var_int32_n(DB *db, const DBT *dbt1, const DBT *dbt2, int n);
TERARK_DLL_EXPORT int bdb_compare_var_int64_n(DB *db, const DBT *dbt1, const DBT *dbt2, int n);

TERARK_DLL_EXPORT size_t bdb_prefix_var_uint32_n(DB *db, const DBT *dbt1, const DBT *dbt2, int n);
TERARK_DLL_EXPORT size_t bdb_prefix_var_uint64_n(DB *db, const DBT *dbt1, const DBT *dbt2, int n);
TERARK_DLL_EXPORT size_t bdb_prefix_var_int32_n(DB *db, const DBT *dbt1, const DBT *dbt2, int n);
TERARK_DLL_EXPORT size_t bdb_prefix_var_int64_n(DB *db, const DBT *dbt1, const DBT *dbt2, int n);

TERARK_DLL_EXPORT int bdb_compare_var_uint32_2(DB *db, const DBT *dbt1, const DBT *dbt2);
TERARK_DLL_EXPORT int bdb_compare_var_uint64_2(DB *db, const DBT *dbt1, const DBT *dbt2);
TERARK_DLL_EXPORT int bdb_compare_var_int32_2(DB *db, const DBT *dbt1, const DBT *dbt2);
TERARK_DLL_EXPORT int bdb_compare_var_int64_2(DB *db, const DBT *dbt1, const DBT *dbt2);

TERARK_DLL_EXPORT size_t bdb_prefix_var_uint32_2(DB *db, const DBT *dbt1, const DBT *dbt2);
TERARK_DLL_EXPORT size_t bdb_prefix_var_uint64_2(DB *db, const DBT *dbt1, const DBT *dbt2);
TERARK_DLL_EXPORT size_t bdb_prefix_var_int32_2(DB *db, const DBT *dbt1, const DBT *dbt2);
TERARK_DLL_EXPORT size_t bdb_prefix_var_int64_2(DB *db, const DBT *dbt1, const DBT *dbt2);

TERARK_DLL_EXPORT int bdb_compare_var_uint32_3(DB *db, const DBT *dbt1, const DBT *dbt2);
TERARK_DLL_EXPORT int bdb_compare_var_uint64_3(DB *db, const DBT *dbt1, const DBT *dbt2);
TERARK_DLL_EXPORT int bdb_compare_var_int32_3(DB *db, const DBT *dbt1, const DBT *dbt2);
TERARK_DLL_EXPORT int bdb_compare_var_int64_3(DB *db, const DBT *dbt1, const DBT *dbt2);

TERARK_DLL_EXPORT size_t bdb_prefix_var_uint32_3(DB *db, const DBT *dbt1, const DBT *dbt2);
TERARK_DLL_EXPORT size_t bdb_prefix_var_uint64_3(DB *db, const DBT *dbt1, const DBT *dbt2);
TERARK_DLL_EXPORT size_t bdb_prefix_var_int32_3(DB *db, const DBT *dbt1, const DBT *dbt2);
TERARK_DLL_EXPORT size_t bdb_prefix_var_int64_3(DB *db, const DBT *dbt1, const DBT *dbt2);

TERARK_DLL_EXPORT int bdb_compare_var_uint32_4(DB *db, const DBT *dbt1, const DBT *dbt2);
TERARK_DLL_EXPORT int bdb_compare_var_uint64_4(DB *db, const DBT *dbt1, const DBT *dbt2);
TERARK_DLL_EXPORT int bdb_compare_var_int32_4(DB *db, const DBT *dbt1, const DBT *dbt2);
TERARK_DLL_EXPORT int bdb_compare_var_int64_4(DB *db, const DBT *dbt1, const DBT *dbt2);

TERARK_DLL_EXPORT size_t bdb_prefix_var_uint32_4(DB *db, const DBT *dbt1, const DBT *dbt2);
TERARK_DLL_EXPORT size_t bdb_prefix_var_uint64_4(DB *db, const DBT *dbt1, const DBT *dbt2);
TERARK_DLL_EXPORT size_t bdb_prefix_var_int32_4(DB *db, const DBT *dbt1, const DBT *dbt2);
TERARK_DLL_EXPORT size_t bdb_prefix_var_int64_4(DB *db, const DBT *dbt1, const DBT *dbt2);

} // extern "C"

template<class T>
inline bt_compare_fcn_type bdb_auto_bt_compare(T*) { return &T::bdb_bt_compare; }

inline bt_compare_fcn_type bdb_auto_bt_compare(terark::var_uint32_t*) { return &bdb_compare_var_uint32; }
inline bt_compare_fcn_type bdb_auto_bt_compare(terark::var_uint64_t*) { return &bdb_compare_var_uint64; }
inline bt_compare_fcn_type bdb_auto_bt_compare(terark::var_int32_t*) { return &bdb_compare_var_int32; }
inline bt_compare_fcn_type bdb_auto_bt_compare(terark::var_int64_t*) { return &bdb_compare_var_int64; }

inline bt_compare_fcn_type bdb_auto_bt_compare(int*) { return &bdb_compare_int32; }
inline bt_compare_fcn_type bdb_auto_bt_compare(unsigned*) { return &bdb_compare_uint32; }
#if ULONG_MAX == 0xFFFFFFFF
inline bt_compare_fcn_type bdb_auto_bt_compare(long*) { return &bdb_compare_int32; }
inline bt_compare_fcn_type bdb_auto_bt_compare(unsigned long*) { return &bdb_compare_uint32; }
#else
inline bt_compare_fcn_type bdb_auto_bt_compare(long*) { return &bdb_compare_int64; }
inline bt_compare_fcn_type bdb_auto_bt_compare(unsigned long*) { return &bdb_compare_uint64; }
#endif
inline bt_compare_fcn_type bdb_auto_bt_compare(boost::long_long_type*) { return &bdb_compare_int64; }
inline bt_compare_fcn_type bdb_auto_bt_compare(boost::ulong_long_type*) { return &bdb_compare_uint64; }

inline bt_compare_fcn_type bdb_auto_bt_compare(float*) { return &bdb_compare_float; }
inline bt_compare_fcn_type bdb_auto_bt_compare(double*) { return &bdb_compare_double; }
inline bt_compare_fcn_type bdb_auto_bt_compare(long double*) { return &bdb_compare_long_double; }

inline bt_compare_fcn_type bdb_auto_bt_compare(std::string*) { return &bdb_locale_mash_strcmp_nocase; }
inline bt_compare_fcn_type bdb_auto_bt_compare(basic_dbt_string<std::string>*) { return &bdb_locale_strcmp_nocase; }

inline bt_compare_fcn_type bdb_auto_bt_compare(TT_PAIR(terark::var_uint32_t)*) { return &bdb_compare_var_uint32_2; }
inline bt_compare_fcn_type bdb_auto_bt_compare(TT_PAIR(terark::var_uint64_t)*) { return &bdb_compare_var_uint64_2; }
inline bt_compare_fcn_type bdb_auto_bt_compare(TT_PAIR(terark::var_int32_t)*) { return &bdb_compare_var_int32_2; }
inline bt_compare_fcn_type bdb_auto_bt_compare(TT_PAIR(terark::var_int64_t)*) { return &bdb_compare_var_int64_2; }

#define TERARK_TUPLE_2(t) boost::tuples::tuple<t,t >
#define TERARK_TUPLE_3(t) boost::tuples::tuple<t,t,t >
#define TERARK_TUPLE_4(t) boost::tuples::tuple<t,t,t,t >

inline bt_compare_fcn_type bdb_auto_bt_compare(TERARK_TUPLE_2(terark::var_uint32_t)*) { return &bdb_compare_var_uint32_2; }
inline bt_compare_fcn_type bdb_auto_bt_compare(TERARK_TUPLE_2(terark::var_uint64_t)*) { return &bdb_compare_var_uint64_2; }
inline bt_compare_fcn_type bdb_auto_bt_compare(TERARK_TUPLE_2(terark::var_int32_t)*) { return &bdb_compare_var_int32_2; }
inline bt_compare_fcn_type bdb_auto_bt_compare(TERARK_TUPLE_2(terark::var_int64_t)*) { return &bdb_compare_var_int64_2; }

inline bt_compare_fcn_type bdb_auto_bt_compare(TERARK_TUPLE_3(terark::var_uint32_t)*) { return &bdb_compare_var_uint32_3; }
inline bt_compare_fcn_type bdb_auto_bt_compare(TERARK_TUPLE_3(terark::var_uint64_t)*) { return &bdb_compare_var_uint64_3; }
inline bt_compare_fcn_type bdb_auto_bt_compare(TERARK_TUPLE_3(terark::var_int32_t)*) { return &bdb_compare_var_int32_3; }
inline bt_compare_fcn_type bdb_auto_bt_compare(TERARK_TUPLE_3(terark::var_int64_t)*) { return &bdb_compare_var_int64_3; }

inline bt_compare_fcn_type bdb_auto_bt_compare(TERARK_TUPLE_4(terark::var_uint32_t)*) { return &bdb_compare_var_uint32_4; }
inline bt_compare_fcn_type bdb_auto_bt_compare(TERARK_TUPLE_4(terark::var_uint64_t)*) { return &bdb_compare_var_uint64_4; }
inline bt_compare_fcn_type bdb_auto_bt_compare(TERARK_TUPLE_4(terark::var_int32_t)*) { return &bdb_compare_var_int32_4; }
inline bt_compare_fcn_type bdb_auto_bt_compare(TERARK_TUPLE_4(terark::var_int64_t)*) { return &bdb_compare_var_int64_4; }

#endif // __cplusplus


#endif // __terark_bdb_util_native_compare_hpp__
