#ifndef __TERARK_C_ADT_META_H__
#define __TERARK_C_ADT_META_H__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
# pragma warning(disable: 4127)
#endif

#include <stddef.h>
#if defined(__GNUC__)
# include <stdint.h>
#endif

#if defined(linux) || defined(__linux) || defined(__linux__)
# include <linux/types.h>
#else
# include <sys/types.h>
#endif

#include "config.h"
#include "field_type.h"
#include "mpool.h"

#ifdef __cplusplus
extern "C" {
#endif

/*--------------------------------------------------------------------------------------*/
/* Function types. */
//typedef void container_type;
typedef unsigned char adt_node;
struct container_type;
struct container_vtab;

/**
 @param vtab  holed state info of compare
 @param cont  per-cont specific data should lies in container_type derived
 */
typedef
int
(*adt_func_compare)(const struct container_vtab* vtab,
					const struct container_type* cont,
					const void* x,
					const void* y
					);

typedef
void
(*adt_func_data_destroy)(const struct container_vtab* vtab,
						 const struct container_type* cont,
						 void* data
						 );

typedef
int
(*adt_func_data_copy)(const struct container_vtab* vtab,
					  const struct container_type* cont,
					  void* dest,
					  const void* src
					  );

typedef
adt_node*
(*adt_func_find)(const struct container_vtab* vtab,
				 const struct container_type* cont,
				 const void* key
				 );

typedef adt_func_find adt_func_lower_bound;
typedef adt_func_find adt_func_upper_bound;

typedef
adt_node*
(*adt_func_equal_range)(const struct container_vtab* vtab,
						const struct container_type* cont,
						const void* key,
						adt_node** upp
						);
typedef
size_t
(*adt_func_data_get_size)(const struct container_vtab* vtab, const void* data);

/*--------------------------------------------------------------------------------------*/

struct container_vtab
{
	struct sallocator* alloc;
	adt_func_compare pf_compare;
	adt_func_find    pf_find;

	/**
	 @brief
	 @note if pf_data_get_size != NULL, actual node size = data_offset + pf_data_get_size(vtab, data)
			in this case, container_vtab.node_size will not be used
	 */
	adt_func_data_get_size pf_data_get_size;
	adt_func_data_copy     pf_data_copy;
	adt_func_data_destroy  pf_data_destroy;

	field_type_t key_type;
	int node_size;   ///< node_size
	int data_size;   ///< app data size
	int node_offset; ///< where the adt_node lies in the whole node, whole node maybe has many index info
	int data_offset; ///< where the user data lies in the whole node, data_offset_of_whole_node
	int key_offset;  ///< where the key lies in the data, key_offset_of_data
	int key_size;

	void* app_data;
};

TERARK_DLL_EXPORT
int
terark_adt_compare_less_tag(const struct container_vtab* vtab,
							const struct container_type* cont,
							const void* x,
							const void* y);

#ifdef __cplusplus
}
#endif

#endif /* __TERARK_C_ADT_META_H__ */

