#include "field_type.h"

#include <assert.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>

#define STATIC_ASSERT(x) { const int xxx = 1 / !!(x); (void)xxx; }

#if defined(_MSC_VER)
/* warning C4189: 'xxx' : local variable is initialized but not referenced */
#  pragma warning(push)
#  pragma warning(disable: 4189)
#endif
int field_type_size(field_type_t t)
{
   STATIC_ASSERT( 4 == sizeof(float))
   STATIC_ASSERT( 8 == sizeof(double))

#ifdef TERARK_C_LONG_DOUBLE_SIZE
	STATIC_ASSERT(TERARK_C_LONG_DOUBLE_SIZE == sizeof(long double))
#endif
	switch (t)
	{
	default:
		return 11111;
	case tev_char: return 1;
	case tev_byte: return 1;
	case tev_uint16:  return 2;
	case tev_int16:   return 2;
	case tev_uint32:  return 4;
	case tev_int32:   return 4;
	case tev_uint64:  return 8;
	case tev_int64:   return 8;

	case tev_float:   return 4;
	case tev_double:  return 8;
#if defined(TERARK_C_LONG_DOUBLE_SIZE)
	case tev_ldouble: return TERARK_C_LONG_DOUBLE_SIZE;
#endif
	case tev_int:     return sizeof(int);
	case tev_uint:    return sizeof(unsigned int);
	case tev_long:    return sizeof(long);
	case tev_ulong:   return sizeof(unsigned long);
	case tev_ptr:     return sizeof(void*);
	}
}

#if defined(_MSC_VER)
#  pragma warning(pop)
#endif

const char* field_type_name_str(field_type_t t)
{
	switch (t)
	{
	default:
		return "unknow";
	case tev_char: return "char";
	case tev_byte: return "byte";
	case tev_uint16:  return "uint16";
	case tev_int16:   return "int16";
	case tev_uint32:  return "uint32";
	case tev_int32:   return "int32";
	case tev_uint64:  return "uint64";
	case tev_int64:   return "int64";
	case tev_float:   return "float";
	case tev_double:  return "double";
#if defined(TERARK_C_LONG_DOUBLE_SIZE)
	case tev_ldouble: return "long double";
#endif
	case tev_ptr: return "pointer";
	}
}


field_type_t internal_field_type(field_type_t field_type)
{
	switch (field_type)
	{
#define internal_field_type_case(old_tev, type) \
	case old_tev: \
		if (0) {} \
		else if (4 == sizeof(type)) return tev_uint32; \
		else if (8 == sizeof(type)) return tev_uint64; \
		else { \
			fprintf(stderr, "unsupported pointer length = %d\n", (int)sizeof(type)); \
			abort(); \
		} \
		break

	internal_field_type_case(tev_ptr, void*);
	internal_field_type_case(tev_int, int);
	internal_field_type_case(tev_uint, unsigned int);
	internal_field_type_case(tev_long, long);
	internal_field_type_case(tev_ulong, unsigned long);
	case tev_blob:
		return tev_blob;
	case tev_app:
		return tev_app;

	default:
		if ((int)field_type < 0 || (int)field_type >= tev_type_count__)
		{
			fprintf(stderr, "invalid argument field_type=%d\n", field_type);
			abort();
		}
		return field_type;
	}
}
