// input MACRO params:
//   TERARK_FUNC_NAME

static const array_handler_r1k CAT_TOKEN2(_S_tab_, TERARK_FUNC_NAME)[] =
{
#define MAKE_FUNC_NAME(tev) CAT_TOKEN3(TERARK_FUNC_NAME,_,tev)
#include "fun_field_type_tab.h"
#undef MAKE_FUNC_NAME
	NULL
};

ptrdiff_t CAT_TOKEN2(terark_, TERARK_FUNC_NAME) // fun name
	(const void* first, ptrdiff_t count, ptrdiff_t elem_size, ptrdiff_t field_offset, field_type_t field_type, ...)
{
	field_type_t ft2 = internal_field_type(field_type);
	array_handler_r1k pf = CAT_TOKEN2(_S_tab_, TERARK_FUNC_NAME)[ft2];
	ptrdiff_t ret;
	va_list key;
	va_start(key, field_type);
#ifdef TERARK_ALGORITHM_DEBUG
	g_first = (char*)first;
	g_last = (char*)first + elem_size * count;
#endif
	if (field_offset + field_type_size(field_type) > elem_size)
	{
		fprintf(stderr, "invalid argument, elem_size=%d, field[type=[%d]%s,offset=%d]\n"
			, (int)elem_size, field_type, field_type_name_str(field_type), (int)field_offset);
	}
	ret = (*pf)((const char*)first, count, elem_size, field_offset, key);
	va_end(key);
	return ret;
}

static const array_handler_r1k CAT_TOKEN2(Ptr_S_tab_, TERARK_FUNC_NAME)[] =
{
#define MAKE_FUNC_NAME(tev) CAT_TOKEN4(TERARK_FUNC_NAME,_,tev, _byptr)
#include "fun_field_type_tab.h"
#undef MAKE_FUNC_NAME
	NULL
};

ptrdiff_t CAT_TOKEN3(terark_, TERARK_FUNC_NAME, _p) // fun name
	(const void* first, ptrdiff_t count, ptrdiff_t field_offset, field_type_t field_type, ...)
{
	field_type_t ft2 = internal_field_type(field_type);
	array_handler_r1k pf = CAT_TOKEN2(Ptr_S_tab_, TERARK_FUNC_NAME)[ft2];
	ptrdiff_t ret;
	va_list key;
	va_start(key, field_type);
#ifdef TERARK_ALGORITHM_DEBUG
	g_first = (char*)first;
	g_last = (char*)first + sizeof(char*) * count;
#endif
	ret = (*pf)((const char*)first, count, sizeof(char*), field_offset, key);
	va_end(key);
	return ret;
}

#undef TERARK_FUNC_NAME


