#include "algorithm.h"
#include "common_macro.h"
#include "swap.h"

#include <assert.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef _MSC_VER
/* Always compile this module for speed, not size */
//#pragma optimize("t", on)
#endif

//#if defined(_DEBUG) || !defined(NDEBUG)
#ifdef TERARK_ALGORITHM_DEBUG
char *g_first, *g_last;
#  define CHECK_BOUND(p) assert(p >= g_first && p < g_last)
#  define CHECKED_ASSIGN(x,y) CHECK_BOUND(x), ASSIGN_FROM(x,y)
#  define CHECKED_SWAP(x,y) do {CHECK_BOUND(x); CHECK_BOUND(y); Value_swap(x,y); } while (0)
#else
#  define CHECK_BOUND(p)
#  define CHECKED_ASSIGN ASSIGN_FROM
#  define CHECKED_SWAP Value_swap
#endif

//////////////////////////////////////////////////////////////////////////////////////////

#define STATIC_ASSERT(x) { const int xxx = 1 / !!(x); }

#if defined(_MSC_VER)
/* warning C4189: 'xxx' : local variable is initialized but not referenced */
#  pragma warning(push)
#  pragma warning(disable: 4189)
#endif

static void check_args(ptrdiff_t elem_size, ptrdiff_t field_offset, field_type_t field_type)
{
	if (elem_size > TERARK_C_MAX_VALUE_SIZE)
	{
		fprintf(stderr, "invalid argument, elem_size=%d too large, max support is %d.\n"
			, (int)elem_size, TERARK_C_MAX_VALUE_SIZE
			);
		abort();
	}
	else if (elem_size < field_type_size(field_type))
	{
		fprintf(stderr, "invalid argument, field[type=%d[%s], size=%d], elem_size=[%d]\n",
			field_type, field_type_name_str(field_type), field_type_size(field_type), (int)elem_size
			);
		abort();
	}
	else if (elem_size % TERARK_C_STRUCT_ALIGN != 0)
	{
		fprintf(stderr, "invalid argument elem_size=%d, aborted\n", (int)elem_size);
		abort();
	}
	if (field_offset % field_type_size(field_type) != 0)
	{
		fprintf(stderr, "field not aligned, field[offset=%d, size=%d]\n", (int)field_offset, field_type_size(field_type));
	}
	if (field_offset + field_type_size(field_type) > elem_size)
	{
		fprintf(stderr, "invalid argument, elem_size=%d, field[type=[%d]%s,offset=%d]\n"
			, (int)elem_size, field_type, field_type_name_str(field_type), (int)field_offset);
	}
}

void_handler get_heandler(const void_handler* ptab, ptrdiff_t elem_size, field_type_t field_type)
{
	void_handler pf = NULL;
	field_type = internal_field_type(field_type);
	assert(elem_size <= TERARK_C_MAX_VALUE_SIZE);
	if (elem_size < TERARK_C_STRUCT_ALIGN)
	{
		switch (elem_size)
		{
		default:
			pf = NULL;
			break;
		case 1:
		case 2:
			pf = ptab[field_type + (elem_size-1)*tev_type_count__];
			break;
#if TERARK_C_STRUCT_ALIGN > 4
		case 4:
			pf = ptab[field_type + (3-1)*tev_type_count__];
			break;
#endif
#if TERARK_C_STRUCT_ALIGN > 8
		case 8:
			pf = ptab[field_type + (4-1)*tev_type_count__];
			break;
#endif
#if TERARK_C_STRUCT_ALIGN > 16
		case 8:
			pf = ptab[field_type + (5-1)*tev_type_count__];
			break;
#endif
		}
	}
	else
	{
#if TERARK_C_STRUCT_ALIGN == 4
#define SKIP_TAB_ROWS 2
#elif TERARK_C_STRUCT_ALIGN == 8
#define SKIP_TAB_ROWS 3
#elif TERARK_C_STRUCT_ALIGN == 16
#define SKIP_TAB_ROWS 4
#else
#error not supported "TERARK_C_STRUCT_ALIGN" value
#endif
		ptrdiff_t fun_idx = (elem_size / TERARK_C_STRUCT_ALIGN + SKIP_TAB_ROWS - 1) * tev_type_count__ + field_type;
		pf = ptab[fun_idx];
	}
	if (0 == pf)
	{
		fprintf(stderr, "internal error, can not find sort function\n");
		fprintf(stderr, "\tfield[type=%d[%s], size=%d], elem_size=[%d]\n",
			field_type, field_type_name_str(field_type), field_type_size(field_type), (int)elem_size
			);
		abort();
	}
	return pf;
}

//////////////////////////////////////////////////////////////////////////////////////////
// generate concrete function

/*
#define _Med3           CAT_TOKEN2(_Med3_,FUN_SUFFIX)
void _Med3(char* _First, char* _Mid, char* _Last, ptrdiff_t field_offset)
{	// sort median of three elements to middle
//	FIELD_TYPE_TYPE* p = (FIELD_TYPE_TYPE*)_First;
	CHECK_BOUND(_First);
	CHECK_BOUND(_Mid);
	CHECK_BOUND(_Last);
	if (VAL_LESS_THAN(_Mid, _First))
		Value_swap(_Mid, _First);
	if (VAL_LESS_THAN(_Last, _Mid))
		Value_swap(_Last, _Mid);
	if (VAL_LESS_THAN(_Mid, _First))
		Value_swap(_Mid, _First);
}
*/

#define _Med3(_First, _Mid, _Last) \
	if (VAL_LESS_THAN(_Mid, _First)) Value_swap(_Mid, _First); \
	if (VAL_LESS_THAN(_Last, _Mid))  Value_swap(_Last, _Mid);  \
	if (VAL_LESS_THAN(_Mid, _First)) Value_swap(_Mid, _First)

#define _TM_make_heap   CAT_TOKEN2(make_heap_,FUN_SUFFIX)
#define _TM_push_heap   CAT_TOKEN2(push_heap_,FUN_SUFFIX)
#define _TM_pop_heap    CAT_TOKEN2(pop_heap_ ,FUN_SUFFIX)
#define _TM_sort_heap   CAT_TOKEN2(sort_heap_,FUN_SUFFIX)

#define _Heap_up		CAT_TOKEN2(_Heap_up_,FUN_SUFFIX)
#define _Heap_down_up   CAT_TOKEN2(_Heap_down_up_,FUN_SUFFIX)
#define _TM_Heap_update CAT_TOKEN2(update_heap_,FUN_SUFFIX)

#define _TM_sort_loop   CAT_TOKEN2(_TM_sort_loop_,FUN_SUFFIX)
#define _TM_sort        CAT_TOKEN2(sort_,FUN_SUFFIX)
#define _Median         CAT_TOKEN2(_Median_,FUN_SUFFIX)

#define _Insertion_sort      CAT_TOKEN2(_Insertion_sort_,FUN_SUFFIX)
#define _Unguarded_partition CAT_TOKEN2(_Unguarded_partition_,FUN_SUFFIX)

#define FIELD_TYPE_FILE "fun_elem_size_field_type.h"

//----------------------------------------------------------------------------------------
#define TempBuf    CAT_TOKEN2(TempBuf_, VALUE_SIZE)
#define GET_KEY(p) *(FIELD_TYPE_TYPE*)((char*)(p) + field_offset)
#define FUN_SUFFIX CAT_TOKEN3(VALUE_SIZE,_,FIELD_TYPE_NAME)

#define ELEM_SIZE_FILE_NAME "fun_elem_size_gen.h"

#include "elem_size_loop.h"

#undef ELEM_SIZE_FILE_NAME

#undef FUN_SUFFIX
#undef GET_KEY
#undef TempBuf
//----------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------
// generate sort by ptr functions

#define VALUE_SIZE   sizeof(char*)
#define FAST_ELEM_SIZE sizeof(char*)

typedef char* _ptr_t;
#define TempBuf _ptr_t
#define GET_KEY(pp) *(FIELD_TYPE_TYPE*)(*(char**)(pp) + field_offset)
#define FUN_SUFFIX CAT_TOKEN2(FIELD_TYPE_NAME, _byptr)

// use special swap
#undef Value_swap
#define Value_swap(x,y) do {\
	register char* t = *(char**)(x); *(char**)(x) = *(char**)(y); *(char**)(y) = t;\
  } while(0)

#include "p_field_type_loop.h"

#undef Value_swap

#undef FUN_SUFFIX
#undef GET_KEY
#undef TempBuf
#undef VALUE_SIZE
#undef FAST_ELEM_SIZE
//----------------------------------------------------------------------------------------

#undef FIELD_TYPE_FILE

//////////////////////////////////////////////////////////////////////////////////////////

#undef _TM_make_heap
#undef _TM_push_heap
#undef _TM_pop_heap
#undef _TM_sort_heap

#undef _Heap_up
#undef _Heap_down_up

#undef _TM_sort_loop
#undef _TM_sort
#undef _Med3
#undef _Median

#undef _Insertion_sort
#undef _Unguarded_partition

//////////////////////////////////////////////////////////////////////////////////////////
// generate read only functions:

#define _TM_equal_range   CAT_TOKEN2(equal_range_,FUN_SUFFIX)
#define _TM_lower_bound   CAT_TOKEN2(lower_bound_,FUN_SUFFIX)
#define _TM_upper_bound   CAT_TOKEN2(upper_bound_,FUN_SUFFIX)
#define _TM_binary_search CAT_TOKEN2(binary_search_,FUN_SUFFIX)

#define FAST_ELEM_SIZE elem_size
#define FIELD_TYPE_FILE  "fun_field_type.h"

//----------------------------------------------------------------------------------------
// generate data array functions
#define FUN_SUFFIX		 FIELD_TYPE_NAME
#define GET_KEY(p)		 *(FIELD_TYPE_TYPE*)((char*)(p) + field_offset)

#include "p_field_type_loop.h"

#undef GET_KEY
#undef FUN_SUFFIX
#undef FAST_ELEM_SIZE
//----------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------
// generate pointer array functions
#define FUN_SUFFIX		 CAT_TOKEN2(FIELD_TYPE_NAME, _byptr)
#define GET_KEY(pp)		 *(FIELD_TYPE_TYPE*)(*(char**)(pp) + field_offset)
#define FAST_ELEM_SIZE sizeof(char*)

#ifdef _MSC_VER
#  pragma warning(push)
#  pragma warning(disable: 4100)
#  include "p_field_type_loop.h"
#  pragma warning(pop)
#else
#  include "p_field_type_loop.h"
#endif

#undef FAST_ELEM_SIZE
#undef GET_KEY
#undef FUN_SUFFIX
//----------------------------------------------------------------------------------------

#undef FIELD_TYPE_FILE

#undef _TM_binary_search
#undef _TM_equal_range
#undef _TM_lower_bound
#undef _TM_upper_bound

// end of generate read only functions
//////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////
// generate interface functions

#define ELEM_SIZE_FILE_NAME "fun_field_type_tab.h"

#define TERARK_FUNC_NAME sort
#include "array_handler_body.h"

#define TERARK_FUNC_NAME push_heap
#include "array_handler_body.h"

#define TERARK_FUNC_NAME pop_heap
#include "array_handler_body.h"

#define TERARK_FUNC_NAME make_heap
#include "array_handler_body.h"

#define TERARK_FUNC_NAME sort_heap
#include "array_handler_body.h"

static const array_update_heap _S_tab_update_heap[] =
{
#define MAKE_FUNC_NAME(tev) CAT_TOKEN5(update_heap,_,VALUE_SIZE,_,tev)
#include "elem_size_loop.h"
#undef MAKE_FUNC_NAME
	NULL
};

void terark_update_heap(void* first, ptrdiff_t _IdxUpdated, ptrdiff_t count, ptrdiff_t elem_size, ptrdiff_t field_offset, field_type_t field_type)
{
	array_update_heap pf;
	check_args(elem_size, field_offset, field_type);
	pf = (array_update_heap)get_heandler(_S_tab_update_heap, elem_size, field_type);
#ifdef TERARK_ALGORITHM_DEBUG
	g_first = (char*)first;
	g_last = (char*)first + elem_size * count;
#endif
	(*pf)((char*)first, _IdxUpdated, count, field_offset);
}

static const array_update_heap Ptr_S_tab_update_heap[] =
{
#define MAKE_FUNC_NAME(tev) CAT_TOKEN4(update_heap,_,tev,_byptr)
#include "fun_field_type_tab.h"
#undef MAKE_FUNC_NAME
	NULL
};

void terark_update_heap_p(void* first, ptrdiff_t _IdxUpdated, ptrdiff_t count, ptrdiff_t field_offset, field_type_t field_type)
{
	field_type_t ft2 = internal_field_type(field_type);
	array_update_heap pf = Ptr_S_tab_update_heap[ft2];
#ifdef TERARK_ALGORITHM_DEBUG
	g_first = (char*)first;
	g_last = (char*)first + sizeof(void*) * count;
#endif
	(*pf)((char*)first, _IdxUpdated, count, field_offset);
}

//----------------------------------------------------------------------------------------
// ignore VALUE_SIZE
#define VALUE_SIZE TERARK_C_MAX_VALUE_SIZE

#define TERARK_FUNC_NAME lower_bound
#include "array_handler_r1k_body.h"

#define TERARK_FUNC_NAME upper_bound
#include "array_handler_r1k_body.h"

#define TERARK_FUNC_NAME binary_search
#include "array_handler_r1k_body.h"

static array_handler_r2k _S_tab_equal_range[] =
{
#define MAKE_FUNC_NAME(tev) CAT_TOKEN2(equal_range_,tev)
#include "fun_field_type_tab.h"
#undef MAKE_FUNC_NAME
	NULL
};
ptrdiff_t terark_equal_range(const void* first, ptrdiff_t count, ptrdiff_t elem_size, ptrdiff_t field_offset, field_type_t field_type, ptrdiff_t* upp, ...)
{
	field_type_t ft2 = internal_field_type(field_type);
	array_handler_r2k pf = _S_tab_equal_range[ft2];
	ptrdiff_t ret;
	va_list key;
	va_start(key, upp);
	ret = (*pf)((const char*)first, count, elem_size, field_offset, upp, key);
	va_end(key);
	return ret;
}

static array_handler_r2k Ptr_S_tab_equal_range[] =
{
#define MAKE_FUNC_NAME(tev) CAT_TOKEN3(equal_range_,tev,_byptr)
#include "p_fun_field_type_tab.h"
#undef MAKE_FUNC_NAME
	NULL
};
ptrdiff_t terark_equal_range_p(const void* first, ptrdiff_t count, ptrdiff_t field_offset, field_type_t field_type, ptrdiff_t* upp, ...)
{
	field_type_t ft2 = internal_field_type(field_type);
	array_handler_r2k pf = Ptr_S_tab_equal_range[ft2];
	ptrdiff_t ret;
	va_list key;
	va_start(key, upp);
	ret = (*pf)((const char*)first, count, sizeof(char*), field_offset, upp, key);
	va_end(key);
	return ret;
}

#undef ELEM_SIZE_FILE_NAME

//////////////////////////////////////////////////////////////////////////

void terark_arraycb_init(terark_arraycb* self, ptrdiff_t elem_size, ptrdiff_t field_offset, field_type_t field_type, int is_byptr)
{
	field_type_t ft2 = internal_field_type(field_type);
	check_args(elem_size, field_offset, field_type);
	if (is_byptr)
	{
		// ignore elem_size
		self->sort      = (array_handler)Ptr_S_tab_sort[ft2];
		self->sort_heap = (array_handler)Ptr_S_tab_sort_heap[ft2];
		self->make_heap = (array_handler)Ptr_S_tab_make_heap[ft2];
		self->push_heap = (array_handler)Ptr_S_tab_push_heap[ft2];
		self-> pop_heap = (array_handler)Ptr_S_tab_pop_heap[ft2];
		self->update_heap = (array_update_heap)Ptr_S_tab_update_heap[ft2];

		self->binary_search = (array_handler_r1k)Ptr_S_tab_binary_search[ft2];
		self->lower_bound   = (array_handler_r1k)Ptr_S_tab_lower_bound[ft2];
		self->upper_bound   = (array_handler_r1k)Ptr_S_tab_upper_bound[ft2];
		self->equal_range   = (array_handler_r2k)Ptr_S_tab_equal_range[ft2];
	}
	else
	{
		self->sort      = (array_handler)get_heandler(_S_tab_sort, elem_size, field_type);
		self->sort_heap = (array_handler)get_heandler(_S_tab_sort_heap, elem_size, field_type);
		self->make_heap = (array_handler)get_heandler(_S_tab_make_heap, elem_size, field_type);
		self->push_heap = (array_handler)get_heandler(_S_tab_push_heap, elem_size, field_type);
		self-> pop_heap = (array_handler)get_heandler(_S_tab_pop_heap, elem_size, field_type);
		self->update_heap = (array_update_heap)get_heandler(_S_tab_update_heap, elem_size, field_type);

		self->binary_search = (array_handler_r1k)_S_tab_binary_search[ft2];
		self->lower_bound   = (array_handler_r1k)_S_tab_lower_bound[ft2];
		self->upper_bound   = (array_handler_r1k)_S_tab_upper_bound[ft2];
		self->equal_range   = (array_handler_r2k)_S_tab_equal_range[ft2];
	}
	self->type_str       = field_type_name_str(field_type);
	self->field_type_idx = ft2;
	self->field_type     = field_type;
	self->elem_size      = elem_size;
	self->field_offset   = field_offset;
	self->is_byptr       = is_byptr;
}

ptrdiff_t terark_acb_binary_search(const terark_arraycb* acb, const void* first, ptrdiff_t count, ...)
{
	ptrdiff_t ret;
	va_list key;
	va_start(key, count);
	ret = acb->binary_search(first, count, acb->elem_size, acb->field_offset, key);
	va_end(key);
	return ret;
}

ptrdiff_t terark_acb_lower_bound(const terark_arraycb* acb, const void* first, ptrdiff_t count, ...)
{
	ptrdiff_t ret;
	va_list key;
	va_start(key, count);
	ret = acb->lower_bound(first, count, acb->elem_size, acb->field_offset, key);
	va_end(key);
	return ret;
}

ptrdiff_t terark_acb_upper_bound(const terark_arraycb* acb, const void* first, ptrdiff_t count, ...)
{
	ptrdiff_t ret;
	va_list key;
	va_start(key, count);
	ret = acb->upper_bound(first, count, acb->elem_size, acb->field_offset, key);
	va_end(key);
	return ret;
}

ptrdiff_t terark_acb_equal_range(const terark_arraycb* acb, const void* first, ptrdiff_t count, ptrdiff_t* upp, ...)
{
	ptrdiff_t ret;
	va_list key;
	va_start(key, upp);
	ret = acb->equal_range(first, count, acb->elem_size, acb->field_offset, upp, key);
	va_end(key);
	return ret;
}

