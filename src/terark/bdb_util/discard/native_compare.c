#include "native_compare.h"
#include <string.h> // for memcpy

#ifdef __cplusplus
extern "C" {
#endif


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

int bdb_strcmp_nocase(DB *db, const DBT *dbt1, const DBT *dbt2)
{
	int ret =
#if defined(_WIN32) || defined(_WIN64)
	_strnicmp
#else
	strncasecmp
#endif
	((char*)dbt1->data, (char*)dbt2->data,
		dbt1->size < dbt2->size ? dbt1->size : dbt2->size);

	return 0 == ret ? dbt1->size - dbt2->size : ret;
}

int bdb_compare_string(DB *db, const DBT *dbt1, const DBT *dbt2)
{
	int ret = strncmp((char*)dbt1->data, (char*)dbt2->data,
		dbt1->size < dbt2->size ? dbt1->size : dbt2->size);
	return 0 == ret ? dbt1->size - dbt2->size : ret;
}


#ifdef __cplusplus
}
#endif

