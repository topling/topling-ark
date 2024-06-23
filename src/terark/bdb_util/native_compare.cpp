#include "native_compare.hpp"
#include <string.h> // for memcpy
#include <string>
#include <terark/io/var_int.hpp>
#include <terark/io/DataIO.hpp>
#include <terark/io/MemStream.hpp>

extern "C" {

#if (defined(_WIN32) || defined(_WIN64)) && !defined(__CYGWIN__)
	#define strncasecmp _strnicmp
#endif

//! HAS NO string length saved
// DataFormat: var_int + string_content
int bdb_cmp_var_uint32_str(DB *db, const DBT *dbt1, const DBT *dbt2)
{
	using namespace terark;

	PortableDataInput<MinMemIO> input1;input1.set(dbt1->data);
	PortableDataInput<MinMemIO> input2;input2.set(dbt2->data);
	var_uint32_t u1, u2;
	input1 >> u1; assert(input1.diff(dbt1->data) < (ptrdiff_t)dbt1->size);
	input2 >> u2; assert(input2.diff(dbt2->data) < (ptrdiff_t)dbt2->size);
	if (u1.t < u2.t) return -1;
	if (u2.t < u1.t) return +1;
	int nx = dbt1->size - input1.diff(dbt1->data);
	int ny = dbt2->size - input2.diff(dbt2->data);
	int ret = strncasecmp((char*)input1.current(), (char*)input2.current(), nx < ny ? nx : ny);
	return 0 == ret ? nx - ny : ret;
}

//! HAS string length saved
// DataFormat: var_int_strlen + string_content + var_int
int bdb_cmp_str_var_uint32(DB *db, const DBT *dbt1, const DBT *dbt2)
{
	using namespace terark;

	PortableDataInput<MinMemIO> input1;input1.set(dbt1->data);
	PortableDataInput<MinMemIO> input2;input2.set(dbt2->data);
	var_uint32_t nx, ny;
	input1 >> nx; assert(input1.diff(dbt1->data) < (ptrdiff_t)dbt1->size);
	input2 >> ny; assert(input2.diff(dbt2->data) < (ptrdiff_t)dbt2->size);
	int ret = strncasecmp((char*)input1.current(), (char*)input2.current(), nx.t < ny.t ? nx.t : ny.t);
	if (0 == ret) {
		if (nx.t == ny.t) {
			var_uint32_t u1, u2;
			input1 >> u1;
			input2 >> u2;
			if (u1.t < u2.t) return -1;
			if (u2.t < u1.t) return +1;
			return 0;
		} else
			return nx.t - ny.t;
	} else
		return ret;
}

inline
int bdb_mash_strcmp_impl(DB *db, const DBT *dbt1, const DBT *dbt2,
						    int (*fstrncmp)(const char* x, const char* y, size_t n)
						    )
{
	using namespace terark;
	PortableDataInput<MinMemIO> input1;input1.set(dbt1->data);
	PortableDataInput<MinMemIO> input2;input2.set(dbt2->data);
	var_uint32_t nx, ny;
	input1 >> nx; assert(input1.diff(dbt1->data) < (ptrdiff_t)dbt1->size);
	input2 >> ny; assert(input2.diff(dbt2->data) < (ptrdiff_t)dbt2->size);
	assert(nx.t < INT_MAX);
	assert(ny.t < INT_MAX);
	int ret = fstrncmp((char*)input1.current(), (char*)input2.current(), nx.t < ny.t ? nx.t : ny.t);
	if (0 == ret) {
		return nx.t - ny.t;
	} else
		return ret;
}

int bdb_mash_strcmp_case(DB *db, const DBT *dbt1, const DBT *dbt2)
{
	return bdb_mash_strcmp_impl(db, dbt1, dbt2, strncmp);
}

int bdb_mash_strcmp_nocase(DB *db, const DBT *dbt1, const DBT *dbt2)
{
	return bdb_mash_strcmp_impl(db, dbt1, dbt2, strncasecmp);
}

inline
int bdb_locale_mash_strcmp_impl(DB *db, const DBT *dbt1, const DBT *dbt2,
						    int (*fstrncmp)(const char* x, const char* y, size_t n1, size_t n2)
						    )
{
	using namespace terark;
	PortableDataInput<MinMemIO> input1;input1.set(dbt1->data);
	PortableDataInput<MinMemIO> input2;input2.set(dbt2->data);
	var_uint32_t nx, ny;
	input1 >> nx; assert(input1.diff(dbt1->data) < (ptrdiff_t)dbt1->size);
	input2 >> ny; assert(input2.diff(dbt2->data) < (ptrdiff_t)dbt2->size);
	assert(nx.t < INT_MAX);
	assert(ny.t < INT_MAX);
	int ret = fstrncmp((char*)input1.current(), (char*)input2.current(), nx.t, ny.t);
	if (0 == ret) {
		return nx.t - ny.t;
	} else
		return ret;
}

static int _S_strncasecoll(const char* x, const char* y, size_t n1, size_t n2)
{
	using namespace std;
	int ret = strncasecmp(x, y, min(n1, n2));
	if (ret == 0)
		return n1 < n2 ? -1 : n2 < n1 ? +1 : 0;
	else
		return ret;
}

static int _S_strncoll(const char* x, const char* y, size_t n1, size_t n2)
{
	using namespace std;
	int ret = strncmp(x, y, min(n1, n2));
	if (ret == 0)
		return n1 < n2 ? -1 : n2 < n1 ? +1 : 0;
	else
		return ret;
}

/**
 * @brief 默认的 string compare 函数
 *
 * 将 string 转成 gb18030，再进行比较（不区分大小写）
 * 这个比较函数按 string 的序列化形式（额外保存了字符串尺寸，但原则上不需要保存尺寸，请用 dbt_string 作为 Key）
 */
int bdb_locale_mash_strcmp_nocase(DB *db, const DBT *dbt1, const DBT *dbt2)
{
	return bdb_locale_mash_strcmp_impl(db, dbt1, dbt2, &_S_strncasecoll);
}

/**
 * @brief 区分大小写 @see bdb_locale_mash_strcmp_nocase
 *
 * 将 string 转成 gb18030，再进行比较（不区分大小写）
 * 这个比较函数按 string 的序列化形式（额外保存了字符串尺寸，但原则上不需要保存尺寸，请用 dbt_string 作为 Key）
 */
int bdb_locale_mash_strcmp_case(DB *db, const DBT *dbt1, const DBT *dbt2)
{
	return bdb_locale_mash_strcmp_impl(db, dbt1, dbt2, &_S_strncoll);
}

inline
int bdb_locale_strcmp_impl(DB *db, const DBT *dbt1, const DBT *dbt2,
						    int (*fstrncmp)(const char* x, const char* y, size_t n1, size_t n2)
						    )
{
	using namespace std;
	size_t n1 = dbt1->size;
	size_t n2 = dbt2->size;
	int cmp = fstrncmp((char*)dbt1->data, (char*)dbt2->data, n1, n2);
	if (0 == cmp)
		return n1 - n2;
	return cmp;
}

/**
 * @brief 默认的 dbt_string compare 函数
 *
 * 将 string 转成 gb18030，再进行比较（不区分大小写）
 */
int bdb_locale_strcmp_nocase(DB *db, const DBT *dbt1, const DBT *dbt2)
{
	return bdb_locale_strcmp_impl(db, dbt1, dbt2, &_S_strncasecoll);
}

/**
 * 区分大小写，@see bdb_locale_strcmp_nocase
 */
int bdb_locale_strcmp_case(DB *db, const DBT *dbt1, const DBT *dbt2)
{
	return bdb_locale_strcmp_impl(db, dbt1, dbt2, &_S_strncoll);
}

#define Name uint32
#define Type boost::uint32_t
#include "native_compare_inc.hpp"

#define Name int32
#define Type boost::int32_t
#include "native_compare_inc.hpp"

#define Name uint64
#define Type boost::uint64_t
#include "native_compare_inc.hpp"

#define Name int64
#define Type boost::int64_t
#include "native_compare_inc.hpp"

size_t bdb_common_byte_prefix(DB *db, const DBT *dbt1, const DBT *dbt2)
{
	unsigned char* p1 = (unsigned char*)(dbt1->data);
	unsigned char* p2 = (unsigned char*)(dbt2->data);
	int nn;
	for (nn = dbt1->size < dbt2->size ? dbt1->size : dbt2->size ; nn; --nn, ++p1, ++p2)
		if (*p1 != *p2)
			return p1 - (unsigned char*)(dbt1->data) + 1;

	if (dbt1->size < dbt2->size)
		return dbt1->size + 1;
	else if (dbt2->size < dbt1->size)
		return dbt2->size + 1;
	else
		return dbt1->size; // equal
}

size_t bdb_fixed_key_prefix(DB *db, const DBT *dbt1, const DBT *dbt2)
{
	return 1;
}

#define GEN_BDB_COMPARE(Type, Name) \
int bdb_compare_##Name(DB *db, const DBT *dbt1, const DBT *dbt2) \
{ \
	Type x1, x2; \
	memcpy(&x1, dbt1->data, sizeof(Type)); \
	memcpy(&x2, dbt2->data, sizeof(Type)); \
	if (x1 < x2) return -1; \
	if (x2 < x1) return  1; \
	return 0; \
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

GEN_BDB_COMPARE(u_int32_t, uint32)
GEN_BDB_COMPARE(u_int64_t, uint64)
/*
#ifdef BOOST_CSTDINT_HPP
GEN_BDB_COMPARE(boost::int32_t, int32)
GEN_BDB_COMPARE(boost::int64_t, int64)
#else
GEN_BDB_COMPARE(int32_t, int32)
GEN_BDB_COMPARE(int64_t, int64)
#endif
GEN_BDB_COMPARE(float, float)
GEN_BDB_COMPARE(double, double)
GEN_BDB_COMPARE(long double, long_double)
*/

int bdb_strcmp_nocase(DB *db, const DBT *dbt1, const DBT *dbt2)
{
	int ret = strncasecmp((char*)dbt1->data, (char*)dbt2->data,
		dbt1->size < dbt2->size ? dbt1->size : dbt2->size);

	return 0 == ret ? dbt1->size - dbt2->size : ret;
}

int bdb_compare_string(DB *db, const DBT *dbt1, const DBT *dbt2)
{
	int ret = strncmp((char*)dbt1->data, (char*)dbt2->data,
		dbt1->size < dbt2->size ? dbt1->size : dbt2->size);
	return 0 == ret ? dbt1->size - dbt2->size : ret;
}


} // extern "C"


