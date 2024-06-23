#ifndef __terark_c_algorithm_h__
#define __terark_c_algorithm_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
# pragma warning(disable: 4127)
#endif

#include "config.h"
#include <stdarg.h>
#include <stddef.h>

#include "field_type.h"

#ifdef __cplusplus
extern "C" {
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

typedef void (*void_handler)();

typedef void (*array_handler)(char* _First, ptrdiff_t count, ptrdiff_t field_offset);

typedef void (*array_handler_v)(char* _First, ptrdiff_t count, ptrdiff_t field_offset, va_list val);

typedef ptrdiff_t  (*array_handler_r1k)(const char* _First, ptrdiff_t count, ptrdiff_t elem_size, ptrdiff_t field_offset, va_list key);
typedef ptrdiff_t  (*array_handler_r2k)(const char* _First, ptrdiff_t count, ptrdiff_t elem_size, ptrdiff_t field_offset, ptrdiff_t* res, va_list key);

//! when _First[_IdxUpdated] was updated, call this function to keep heap form
typedef void (*array_update_heap)(char* _First, ptrdiff_t _IdxUpdated, ptrdiff_t _Bottom, ptrdiff_t field_offset);

typedef struct terark_arraycb
{
	array_handler sort;

	array_handler sort_heap;
	array_handler make_heap;
	array_handler push_heap;
	array_handler  pop_heap;
	array_update_heap update_heap;

	array_handler_r1k binary_search;
	array_handler_r1k lower_bound;
	array_handler_r1k upper_bound;
	array_handler_r2k equal_range;

	const char*  type_str;
	ptrdiff_t    elem_size;
	ptrdiff_t    field_offset;
	int          is_byptr;
	field_type_t field_type;
	field_type_t field_type_idx;
} terark_arraycb;

void TERARK_DLL_EXPORT terark_arraycb_init(terark_arraycb* self, ptrdiff_t elem_size, ptrdiff_t field_offset, field_type_t field_type, int is_byptr);

#define terark_offsetp(ptr, field_name) ((char*)&((ptr)->field_name) - (char*)(ptr))

#define terark_arraycb_init_m(self, first, field_type) \
		terark_arraycb_init(self, sizeof(*(first)), terark_offsetp(first, field_name), field_type, false)

#define terark_arraycb_init_t(self, elem_type, field_type) \
		terark_arraycb_init(self, sizeof(elem_type), (ptrdiff_t)&((elem_type*)0)->field_name, field_type, false)

#define terark_arraycb_handle(acb, fun, first, count) (acb).fun(first, count, (acb).field_offset)

#define terark_acb_sort(     acb, first, count) terark_arraycb_handle(acb, sort     , first, count)
#define terark_acb_sort_heap(acb, first, count) terark_arraycb_handle(acb, sort_heap, first, count)
#define terark_acb_make_heap(acb, first, count) terark_arraycb_handle(acb, make_heap, first, count)
#define terark_acb_push_heap(acb, first, count) terark_arraycb_handle(acb, push_heap, first, count)
#define terark_acb_pop_heap( acb, first, count) terark_arraycb_handle(acb,  pop_heap, first, count)

//! @{
//! va_arg ... is Key
ptrdiff_t TERARK_DLL_EXPORT terark_acb_binary_search(const terark_arraycb* acb, const void* first, ptrdiff_t count, ...);
ptrdiff_t TERARK_DLL_EXPORT terark_acb_lower_bound(const terark_arraycb* acb, const void* first, ptrdiff_t count, ...);
ptrdiff_t TERARK_DLL_EXPORT terark_acb_upper_bound(const terark_arraycb* acb, const void* first, ptrdiff_t count, ...);
ptrdiff_t TERARK_DLL_EXPORT terark_acb_equal_range(const terark_arraycb* acb, const void* first, ptrdiff_t count, ptrdiff_t* upp, ...);
//@}

//////////////////////////////////////////////////////////////////////////


void TERARK_DLL_EXPORT terark_sort(void* first, ptrdiff_t count, ptrdiff_t elem_size, ptrdiff_t field_offset, field_type_t field_type);

void TERARK_DLL_EXPORT terark_pop_heap(void* first, ptrdiff_t count, ptrdiff_t elem_size, ptrdiff_t field_offset, field_type_t field_type);
void TERARK_DLL_EXPORT terark_push_heap(void* first, ptrdiff_t count, ptrdiff_t elem_size, ptrdiff_t field_offset, field_type_t field_type);
void TERARK_DLL_EXPORT terark_make_heap(void* first, ptrdiff_t count, ptrdiff_t elem_size, ptrdiff_t field_offset, field_type_t field_type);
void TERARK_DLL_EXPORT terark_sort_heap(void* first, ptrdiff_t count, ptrdiff_t elem_size, ptrdiff_t field_offset, field_type_t field_type);

ptrdiff_t TERARK_DLL_EXPORT terark_lower_bound(const void* first, ptrdiff_t count, ptrdiff_t elem_size, ptrdiff_t field_offset, field_type_t field_type, ...);
ptrdiff_t TERARK_DLL_EXPORT terark_upper_bound(const void* first, ptrdiff_t count, ptrdiff_t elem_size, ptrdiff_t field_offset, field_type_t field_type, ...);
ptrdiff_t TERARK_DLL_EXPORT terark_equal_range(const void* first, ptrdiff_t count, ptrdiff_t elem_size, ptrdiff_t field_offset, field_type_t field_type, ptrdiff_t* upper, ...);

void TERARK_DLL_EXPORT terark_sort_p(void* first, ptrdiff_t count, ptrdiff_t field_offset, field_type_t field_type);

void TERARK_DLL_EXPORT terark_pop_heap_p(void* first, ptrdiff_t count, ptrdiff_t field_offset, field_type_t field_type);
void TERARK_DLL_EXPORT terark_push_heap_p(void* first, ptrdiff_t count, ptrdiff_t field_offset, field_type_t field_type);
void TERARK_DLL_EXPORT terark_make_heap_p(void* first, ptrdiff_t count, ptrdiff_t field_offset, field_type_t field_type);
void TERARK_DLL_EXPORT terark_sort_heap_p(void* first, ptrdiff_t count, ptrdiff_t field_offset, field_type_t field_type);

ptrdiff_t TERARK_DLL_EXPORT terark_lower_bound_p(const void* first, ptrdiff_t count, ptrdiff_t field_offset, field_type_t field_type, ...);
ptrdiff_t TERARK_DLL_EXPORT terark_upper_bound_p(const void* first, ptrdiff_t count, ptrdiff_t field_offset, field_type_t field_type, ...);
ptrdiff_t TERARK_DLL_EXPORT terark_equal_range_p(const void* first, ptrdiff_t count, ptrdiff_t field_offset, field_type_t field_type, ptrdiff_t* upper, ...);


//---------------------------------------------------------------------------------------
#define terark_sort_field_c(first, count, field_name, field_type) \
	terark_sort(first, count, sizeof(*(first)),	terark_offsetp(first, field_name), field_type)

#define terark_sort_value_c(first, count, field_type) \
	terark_sort(first, count, sizeof(*(first)), 0, field_type)

//---------------------------------------------------------------------------------------
#define terark_sort_field_pc(first, count, field_name, field_type) \
	terark_sort_p(first, count, terark_offsetp(*first, field_name), field_type)

#define terark_sort_value_pc(first, count, field_type) terark_sort_p(first, count, 0, field_type)

//---------------------------------------------------------------------------------------
#define terark_push_heap_field_c(first, count, field_name, field_type) \
	terark_push_heap(first, count, sizeof(*(first)), terark_offsetp(first, field_name), field_type)

#define terark_push_heap_value_c(first, count, field_type) \
	terark_push_heap(first, count, sizeof(*(first)), 0, field_type)

//---------------------------------------------------------------------------------------
#define terark_pop_heap_field_c(first, count, field_name, field_type) \
	terark_pop_heap(first, count, sizeof(*(first)), terark_offsetp(first, field_name), field_type)

#define terark_pop_heap_value_c(first, count, field_type) \
	terark_pop_heap(first, count, sizeof(*(first)), 0, field_type)

//---------------------------------------------------------------------------------------
#define terark_make_heap_field_c(first, count, field_name, field_type) \
	terark_make_heap(first, count, sizeof(*(first)), terark_offsetp(first, field_name), field_type)

#define terark_make_heap_value_c(first, count, field_type) \
	terark_make_heap(first, count, sizeof(*(first)), 0, field_type)

//---------------------------------------------------------------------------------------
#define terark_sort_heap_field_c(first, count, field_name, field_type) \
	terark_sort_heap(first, count, sizeof(*(first)), terark_offsetp(first, field_name), field_type)

#define terark_sort_heap_value_c(first, count, field_type) \
	terark_sort_heap(first, count, sizeof(*(first)), 0, field_type)

//---------------------------------------------------------------------------------------
#define terark_lower_bound_field_c(first, count, field_name, field_type, key) \
	terark_lower_bound(first, count, sizeof(*(first)), terark_offsetp(first, field_name), field_type, key)

#define terark_lower_bound_value_c(first, count, field_type, key) \
	terark_lower_bound(first, count, sizeof(*(first)), 0, field_type, key)

//---------------------------------------------------------------------------------------
#define terark_upper_bound_field_c(first, count, field_name, field_type, key) \
	terark_upper_bound(first, count, sizeof(*(first)), terark_offsetp(first, field_name), field_type, key)

#define terark_upper_bound_value_c(first, count, field_type, key) \
	terark_upper_bound(first, count, sizeof(*(first)), 0, field_type, key)

//---------------------------------------------------------------------------------------
#define terark_equal_range_field_c(first, count, field_name, field_type, upper, key) \
	terark_equal_range(first, count, sizeof(*(first)), terark_offsetp(first, field_name), field_type, upper, key)

#define terark_equal_range_value_c(first, count, field_type, upper, key) \
	terark_equal_range(first, count, sizeof(*(first)), 0, field_type, upper, key)

#ifdef __cplusplus
} // extern "C"

//---------------------------------------------------------------------------------------------
#define terark_sort_field(first, count, field_name) \
	terark_sort_field_c(first, count, field_name, terark_get_field_type(&(first)->field_name))

#define terark_sort_value(first, count) \
	terark_sort(first, count, sizeof(*(first)), 0, terark_get_field_type(first))

#define terark_sort_field_p(first, count, field_name) \
	terark_sort_field_pc(first, count, field_name, terark_get_field_type(&(*(first))->field_name))

#define terark_sort_value_p(first, count) \
	terark_sort_p(first, count, 0, terark_get_field_type(*(first)))

//---------------------------------------------------------------------------------------------
#define terark_push_heap_field(first, count, field_name) \
	terark_push_heap_field_c(first, count, field_name, terark_get_field_type(&(first)->field_name))

#define terark_push_heap_value(first, count) \
	terark_push_heap(first, count, sizeof(*(first)), 0, terark_get_field_type(first))

//---------------------------------------------------------------------------------------------
#define terark_pop_heap_field(first, count, field_name) \
	terark_pop_heap_field_c(first, count, field_name, terark_get_field_type(&(first)->field_name))

#define terark_pop_heap_value(first, count) \
	terark_pop_heap(first, count, sizeof(*(first)), 0, terark_get_field_type(first))

//---------------------------------------------------------------------------------------------
#define terark_make_heap_field(first, count, field_name) \
	terark_make_heap_field_c(first, count, field_name, terark_get_field_type(&(first)->field_name))

#define terark_make_heap_value(first, count) \
	terark_make_heap(first, count, sizeof(*(first)), 0, terark_get_field_type(first))

//---------------------------------------------------------------------------------------------
#define terark_sort_heap_field(first, count, field_name) \
	terark_sort_heap_field_c(first, count, field_name, terark_get_field_type(&(first)->field_name))

#define terark_sort_heap_value(first, count) \
	terark_sort_heap(first, count, sizeof(*(first)), 0, terark_get_field_type(first))

//---------------------------------------------------------------------------------------------
#define terark_lower_bound_field(first, count, field_name, key) \
	terark_lower_bound_field_c(first, count, field_name, terark_get_field_type(&(first)->field_name), key)

#define terark_lower_bound_value(first, count, key) \
	terark_lower_bound(first, count, sizeof(*(first)), 0, terark_get_field_type(first), &key)

//---------------------------------------------------------------------------------------------
#define terark_upper_bound_field(first, count, field_name, key) \
	terark_upper_bound_field_c(first, count, field_name, terark_get_field_type(&(first)->field_name), key)

#define terark_upper_bound_value(first, count, key) \
	terark_upper_bound(first, count, sizeof(*(first)), 0, terark_get_field_type(first), &key)

//---------------------------------------------------------------------------------------------
#define terark_equal_range_field(first, count, field_name, upper, key) \
	terark_equal_range_field_c(first, count, field_name, terark_get_field_type(&(first)->field_name), upper, key)

#define terark_equal_range_value(first, count, upper, key) \
	terark_equal_range(first, count, sizeof(*(first)), 0, terark_get_field_type(first), upper, key)



#endif // __cplusplus

#endif // __terark_c_algorithm_h__


