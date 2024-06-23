/* Originally from libval by blp@gnu.org

 1. trb_node was compressed, a node only use 2 pointer's space
	  tag   bit is trb_node.trb_link[x] & 2,
      color bit is trb_node.trb_link[0] & 1,
	  lock  bit is trb_node.trb_link[1] & 1
 2. add trb_vtab for trb_tree's meta data
 3. add lower_bound/upper_bound/equal_range
 4. erase trb_tranverse, replaced with trb_node, trb_node also used as iterator
 5. replace malloc/free to stl.allocator, for small node, stl.allocator usually use memory pool
 6. basic types[integers&floats] added as primitive supported
      these primitive types use fast find/lower_bound/upper_bound/equal_range
 7. basic types include fixed blob, trb_vtab.key_size is the blob size
 8. allow user insert var size data, in this case, must define pf_data_get_size
    use trb_probe_node to insert user allocated node, this node must be allocated by pf_alloc
 */

#ifndef __TERARK_C_TRB_H__
#define __TERARK_C_TRB_H__

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

#include "../config.hpp"
#include "field_type.h"
#include "mpool.h"

#ifdef __cplusplus
extern "C" {
#endif

struct trb_node
{
	intptr_t trb_link[2];
};
#define TRB_DEFAULT_DATA_OFFSET  sizeof(struct trb_node)

struct trb_tree
{
	struct trb_node *trb_root;
	size_t trb_count;  /* Number of items in tree. */
};
#define trb_data_ref(vtab, node, type)  (*(type*)((vtab)->data_offset + (char*)node))

//not needed, use with 'struct xx' not need forward declare 'struct xx'
struct trb_vtab; ///< forward declaration
struct trb_path_arg; ///< forward declaration

/*--------------------------------------------------------------------------------------*/
/* Function types. */

/**
 @param vtab  holed state info of compare
 @param tree  per-tree specific data should lies in trb_tree derived
 */
typedef
int
(*trb_func_compare)(const struct trb_vtab* vtab,
					const struct trb_tree* tree,
					const void* x,
					const void* y);

typedef
void
(*trb_func_data_destroy)(const struct trb_vtab* vtab,
						 const struct trb_tree* tree,
						 void* data);

typedef
int
(*trb_func_data_copy)(const struct trb_vtab* vtab,
					  const struct trb_tree* tree,
					  void* dest,
					  const void* src);

typedef
struct trb_node*
(*trb_func_find)(const struct trb_vtab* vtab,
				 const struct trb_tree* tree,
				 const void* key);
typedef trb_func_find trb_func_lower_bound;
typedef trb_func_find trb_func_upper_bound;

typedef
struct trb_node*
(*trb_func_equal_range)(const struct trb_vtab* vtab,
						const struct trb_tree* tree,
						const void* key,
						struct trb_node** upp);
typedef
size_t
(*trb_func_data_get_size)(const struct trb_vtab* vtab, const void* data);

/*--------------------------------------------------------------------------------------*/

struct trb_vtab
{
	struct sallocator* alloc;
	trb_func_compare pf_compare;
	trb_func_find    pf_find;

	trb_func_lower_bound pf_lower_bound;
	trb_func_upper_bound pf_upper_bound;
	trb_func_equal_range pf_equal_range;

	int
	(*pf_find_path)(const struct trb_vtab* vtab,
					const struct trb_tree* tree,
					const void* data,
					struct trb_path_arg* arg);

	/**
	 @brief
	 @note if pf_data_get_size != NULL, actual node size = data_offset + pf_data_get_size(vtab, data)
			in this case, trb_vtab.node_size will not be used
	 */
	trb_func_data_get_size pf_data_get_size;
	trb_func_data_copy     pf_data_copy;
	trb_func_data_destroy  pf_data_destroy;

	field_type_t key_type;
	int node_size;   ///< node_size, when node is var-size, this is min size
	int data_size;   ///< app data size
	int node_offset; ///< where the trb_node lies in the whole node, whole node maybe has many index info
	int data_offset; ///< where the user data lies in the whole node, data_offset_of_whole_node
	int key_offset;  ///< where the key lies in the data, key_offset_of_data
	int key_size;
	int path_arg_size; ///< sizeof(struct trb_path_arg), definition of trb_path_arg is invisible for app

	void* app_data;

	void (*pf_sum_add)(void* dest, const void* src);
	void (*pf_sum_sub)(void* dest, const void* src);
	void (*pf_sum_copy)(void* dest, const void* src); ///< only copy the extra 'sum' fields
	void (*pf_sum_init)(void* dest);  ///< for rank, set to 1
	void (*pf_sum_atom_add)(void* dest, const void* x);
	void (*pf_sum_atom_sub)(void* dest, const void* x);
	int  (*pf_sum_comp)(const void* x, const void* y); ///< for check

	struct trb_node* (*pf_get_sum)(const struct trb_vtab*, const struct trb_tree*, const void* key, void* result);
};

TERARK_DLL_EXPORT
int
terark_trb_compare_less_tag(const struct trb_vtab*, const struct trb_tree*, const void* x, const void* y);

/**
 @brief init trb_vtab
 @note
  before calling this function, set vtab members appropriately;
  if data_offset == 0, this function will set
 */
TERARK_DLL_EXPORT
void
trb_vtab_init(struct trb_vtab* vtab, field_type_t key_type);

TERARK_DLL_EXPORT
void
trb_init(struct trb_tree* tree);

TERARK_DLL_EXPORT
struct trb_node*
trb_probe(const struct trb_vtab*, struct trb_tree*, const void* data, int* existed);

TERARK_DLL_EXPORT
struct trb_node*
trb_probe_node(const struct trb_vtab*, struct trb_tree*, struct trb_node* node);

TERARK_DLL_EXPORT
int trb_erase(const struct trb_vtab*, struct trb_tree*, const void* key);

TERARK_DLL_EXPORT
int
trb_copy(const struct trb_vtab*, struct trb_tree* dest, const struct trb_tree* org);

TERARK_DLL_EXPORT
void
trb_destroy(const struct trb_vtab* vtab, struct trb_tree* tree);

TERARK_DLL_EXPORT
struct trb_node*
trb_find_tev_app(const struct trb_vtab* vtab, const struct trb_tree* tree, const void* key);

TERARK_DLL_EXPORT void trb_assert_insert(const struct trb_vtab*, struct trb_tree* , void* data);
TERARK_DLL_EXPORT void trb_assert_erase (const struct trb_vtab*, struct trb_tree* , void* data);

TERARK_DLL_EXPORT struct trb_node* trb_iter_first(ptrdiff_t node_offset, const struct trb_tree*);
/// to last node
TERARK_DLL_EXPORT struct trb_node* trb_iter_last(ptrdiff_t node_offset, const struct trb_tree*);

TERARK_DLL_EXPORT struct trb_node* trb_iter_next(ptrdiff_t node_offset, struct trb_node* iter);
TERARK_DLL_EXPORT struct trb_node* trb_iter_prev(ptrdiff_t node_offset, struct trb_node* iter);

TERARK_DLL_EXPORT void trb_check_sum(const struct trb_vtab*, struct trb_tree*, struct trb_node*);

#ifdef __cplusplus
} // end of extern "C"
#endif

#endif /* __TERARK_C_TRB_H__ */

