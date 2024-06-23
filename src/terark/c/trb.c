/*
  Originally from libavl by blp@gnu.org
  Modified an improved by leipeng
  Now the code is re-created by myself, and is very different from blp's original
 */
//#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "trb.h"
#include "common_macro.h"

#ifdef _MSC_VER
/* Always compile this module for speed, not size */
#pragma optimize("t", on)
#endif

#if defined(_NO_SUPPORT_EMBEDED_LINK)
#	define TrbNode(p) p
#	define TRB_define_node_offset
#	define TRB_root_as_node(tree)  &tree->trb_root
#	define TRB_revert_offset(node) node
#else
#	define TrbNode(p) ((struct trb_node*)((char*)(p) + node_offset))
#	define TRB_define_node_offset  intptr_t node_offset = vtab->node_offset;
#	define TRB_root_as_node(tree)  ((char*)(&tree->trb_root) - node_offset)
#	define TRB_revert_offset(node) (struct trb_node*)((char*)(node) - node_offset)
#endif

#define TRB_is_red(p)     (TrbNode(p)->trb_link[0] & 1)
#define TRB_is_black(p)  !(TrbNode(p)->trb_link[0] & 1)
#define TRB_set_red(p)    TrbNode(p)->trb_link[0] |=  1
#define TRB_set_black(p)  TrbNode(p)->trb_link[0] &= ~1

#define TRB_copy_color(p,q) TrbNode(p)->trb_link[0] = (TrbNode(p)->trb_link[0] & ~1) | (TrbNode(q)->trb_link[0] & 1)
#define TRB_swap_color(p,q) { \
	intptr_t t = TrbNode(p)->trb_link[0] & 1; \
	TRB_copy_color(p, q); \
	TrbNode(q)->trb_link[0] = (TrbNode(q)->trb_link[0] & ~1) | t; \
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#define TRB_get_link(p,idx) ((struct trb_node*)(TrbNode(p)->trb_link[idx] & ~3))
#define TRB_set_link(p,idx,q)  TrbNode(p)->trb_link[idx] = (intptr_t)(q) | (TrbNode(p)->trb_link[idx] & 3)

#define TAG_is_thread(p,idx)  (TrbNode(p)->trb_link[idx] & 2)
#define TAG_is_child(p,idx)  !(TrbNode(p)->trb_link[idx] & 2)

#define TRB_self_is_thread(p)   (p & 2)
#define TRB_self_is_child(p)   !(p & 2)
#define TRB_self_get_ptr(p)     ((struct trb_node*)(p & ~3))

#define TAG_copy(p,ip,q,iq)    TrbNode(p)->trb_link[ip] = (TrbNode(p)->trb_link[ip] & ~2) | (TrbNode(q)->trb_link[iq] & 2)

#define TAG_set_thread(p,idx)  TrbNode(p)->trb_link[idx] |= 2
#define TAG_set_child(p,idx)   TrbNode(p)->trb_link[idx] &= ~2

#define TRB_raw_link(p,idx)  TrbNode(p)->trb_link[idx]

/* Maximum TRB height. */
#ifndef TRB_MAX_HEIGHT
#define TRB_MAX_HEIGHT 80
#endif

#define TRB_GET_DATA_SIZE(vtab, data)  (vtab->pf_data_get_size ? vtab->pf_data_get_size(vtab, data) : (size_t)vtab->data_size)

struct trb_path_arg
{
	int height;                          /* Stack height. */
	int hlow;							 /* Equal Key Node */
#if 0 // defined(_DEBUG) || !defined(NDEBUG)
	struct trb_node* pa[TRB_MAX_HEIGHT]; /* Nodes on stack. */
	unsigned char da[TRB_MAX_HEIGHT];    /* Directions moved from stack nodes. */
#	define TPA_dir(i) arg->da[i]
#	define TPA_ptr(i) arg->pa[i]
#	define TPA_set(i,ptr,dir) arg->pa[i] = (struct trb_node*)(ptr), arg->da[i] = dir
#	define TPA_setptr(i,ptr)  arg->pa[i] = ptr
#	define TPA_lower arg.pa[arg.hlow]
#else
	intptr_t pa[TRB_MAX_HEIGHT];
#	define TPA_dir(i)                    (arg->pa[i] &  1)
#	define TPA_ptr(i) ((struct trb_node*)(arg->pa[i] & ~1))
#	define TPA_set(i,ptr,dir) arg->pa[i] = (intptr_t)(ptr) | dir
#	define TPA_setptr(i,ptr)  arg->pa[i] = (intptr_t)(ptr) | (arg->pa[i] & 1)
#	define TPA_lower ((struct trb_node*)(arg.pa[arg.hlow] & ~1))
#endif
};

int
terark_trb_compare_less_tag(const struct trb_vtab* vtab,
							const struct trb_tree* tree,
							const void* x,
							const void* y)
{
	fprintf(stderr, "fatal: terark_trb_compare_less_tag is being called\n");
	abort();
	return 0;
}

static int
trb_compare_tev_blob(const struct trb_vtab* vtab,
					 const struct trb_tree* tree,
					 const void* x,
					 const void* y)
{
	assert(NULL != vtab);
	assert(NULL != tree);
	assert(NULL != x);
	assert(NULL != y);
	return memcmp(x, y, vtab->key_size);
}

/////////////////////////////////////////////////////////////////////////////////////////////

struct trb_node*
trb_lower_bound_tev_app(const struct trb_vtab* vtab,
						const struct trb_tree* tree,
						const void* key)
{
	intptr_t l = 0;
	assert(tree != NULL && key != NULL);
	if (terark_likely(NULL != tree->trb_root))
	{
		TRB_define_node_offset
		ptrdiff_t key_offset_of_node = vtab->data_offset + vtab->key_offset;
		intptr_t p = (intptr_t)tree->trb_root;
		do {
			p &= ~3;
			{
				int cmp = vtab->pf_compare(vtab, tree, key, (char*)p + key_offset_of_node);
				if (cmp > 0)
					p = TRB_raw_link(p,1);
				else {
					l = p;
					p = TRB_raw_link(p,0);
				}
			}
		} while (TRB_self_is_child(p));
	}
	return (struct trb_node*)l;
}

struct trb_node*
trb_upper_bound_tev_app(const struct trb_vtab* vtab,
						const struct trb_tree* tree,
						const void* key)
{
	intptr_t u = 0;
	assert(tree != NULL && key != NULL);
	if (terark_likely(NULL != tree->trb_root))
	{
		TRB_define_node_offset
		ptrdiff_t key_offset_of_node = vtab->data_offset + vtab->key_offset;
		intptr_t p = (intptr_t)tree->trb_root;
		do {
			p &= ~3;
			{
				int cmp = vtab->pf_compare(vtab, tree, key, (char*)p + key_offset_of_node);
				if (cmp < 0) {
					u = p;
					p = TRB_raw_link(p,0);
				} else
					p = TRB_raw_link(p,1);
			}
		} while (TRB_self_is_child(p));
	}
	return (struct trb_node*)u;
}

struct trb_node*
trb_equal_range_tev_app(const struct trb_vtab* vtab,
						const struct trb_tree* tree,
						const void* key,
						struct trb_node** upp)
{
  intptr_t l = 0, u = 0;
  assert(tree != NULL && key != NULL);
  if (terark_likely(NULL != tree->trb_root))
  {
	TRB_define_node_offset
	intptr_t p = (intptr_t)tree->trb_root;
	ptrdiff_t key_offset_of_node = vtab->data_offset + vtab->key_offset;
	do {
		p &= ~3;
		{
			int cmp = vtab->pf_compare(vtab, tree, key, (char*)p + key_offset_of_node);
			if (cmp > 0) {
				p = TRB_raw_link(p,1);
			} else {
				if (0 == u && cmp < 0)
					u = p;
				l = p;
				p = TRB_raw_link(p,0);
			}
		}
	} while (TRB_self_is_child(p));
	p = TRB_self_get_ptr(u) ? u : (intptr_t)tree->trb_root;
	do {
		p &= ~3;
		{
			int cmp = vtab->pf_compare(vtab, tree, key, (char*)p + key_offset_of_node);
			if (cmp < 0) {
				u = p;
				p = TRB_raw_link(p,0);
			} else
				p = TRB_raw_link(p,1);
		}
	} while (TRB_self_is_child(p));

	*upp = (struct trb_node*)u;
  }
  return (struct trb_node*)l;
}

struct trb_node*
trb_find_tev_app_v0(const struct trb_vtab* vtab,
					const struct trb_tree* tree,
					const void* key)
{
	// if found, this function will call pf_compare at least less 1 times than trb_find_tev_app
	assert(tree != NULL && key != NULL);
	if (terark_likely(NULL != tree->trb_root))
	{
		TRB_define_node_offset
		ptrdiff_t key_offset_of_node = vtab->data_offset + vtab->key_offset;
		intptr_t p = (intptr_t)tree->trb_root;
		do {
			p &= ~3;
			{
				int cmp = vtab->pf_compare(vtab, tree, key, (char*)p + key_offset_of_node);
				if (terark_likely(0 != cmp))
					p = TRB_raw_link(p, cmp > 0);
				else
					return (struct trb_node*)p;
			}
		} while (TRB_self_is_child(p));
	}
	return NULL;
}

struct trb_node*
trb_find_tev_app(const struct trb_vtab* vtab,
				 const struct trb_tree* tree,
				 const void* key)
{
	assert(tree != NULL && key != NULL);
	if (terark_likely(NULL != tree->trb_root))
	{
		TRB_define_node_offset
		ptrdiff_t key_offset_of_node = vtab->data_offset + vtab->key_offset;
		intptr_t p = (intptr_t)tree->trb_root;
		intptr_t l = 0;
		int lcmp = -1;
		do {
			p &= ~3;
			{
				int cmp = vtab->pf_compare(vtab, tree, key, (char*)p + key_offset_of_node);
				if (cmp > 0)
					p = TRB_raw_link(p,1);
				else {
					l = p;
					lcmp = cmp;
					p = TRB_raw_link(p,0);
				}
			}
		} while (TRB_self_is_child(p));

	//	if (0 == lcmp && NULL != l)
		if (0 == lcmp) // test for l can be omitted
			return (struct trb_node*)l;
	}
	return NULL;
}

/**
 @brief
 @return
   if existed, return 1, else return 0
 */
int
trb_find_path_tev_app(const struct trb_vtab* vtab,
					  const struct trb_tree* tree,
					  const void* key,
					  struct trb_path_arg* arg)
{
	TRB_define_node_offset
	int existed = 0;
	int k = 1;
	assert(tree != NULL && key != NULL);

	arg->hlow = 0; // k-1
	TPA_set(0, TRB_root_as_node(tree), 0);

	if (terark_likely(NULL != tree->trb_root))
	{
		ptrdiff_t key_offset_of_node = vtab->data_offset + vtab->key_offset;
		intptr_t p = (intptr_t)tree->trb_root;
	//	intptr_t l = 0;
		int lcmp = -1;
		do {
			p &= ~3;
			{
				int cmp = vtab->pf_compare(vtab, tree, key, (char*)p + key_offset_of_node);
				if (cmp > 0) {
					TPA_set(k, p, 1);
					p = TRB_raw_link(p,1);
				}
				else {
				//	l = p;
					TPA_set(k, p, 0);
					arg->hlow = k;
					lcmp = cmp;
					p = TRB_raw_link(p,0);
				}
				++k;
			}
		} while (TRB_self_is_child(p));

		if (terark_unlikely(k > TRB_MAX_HEIGHT)) {
			fprintf(stderr, "fatal: stack_height[cur=%d, max=%d]\n", k, TRB_MAX_HEIGHT);
			abort();
		}
	//	existed = (0 == lcmp && NULL != l);
		existed = (0 == lcmp); // test for l can be omitted
	}
	arg->height = k;
	return existed;
}

struct trb_node*
trb_get_sum_tev_app(const struct trb_vtab* vtab,
					const struct trb_tree* tree,
					const void* key,
					void* result)
{
	intptr_t l = 0;
	assert(tree != NULL && key != NULL);
	assert(NULL != vtab->pf_sum_add);
	if (terark_likely(NULL != tree->trb_root))
	{
		TRB_define_node_offset
		ptrdiff_t key_offset_of_node = vtab->data_offset + vtab->key_offset;
		ptrdiff_t doff = vtab->data_offset;
		intptr_t p = (intptr_t)tree->trb_root;
		do {
			p &= ~3;
			{
				int cmp = vtab->pf_compare(vtab, tree, key, (char*)p + key_offset_of_node);
				if (cmp > 0) {
					vtab->pf_sum_add(result, (char*)(p + doff));
					p = TRB_raw_link(p,1);
					if (TRB_self_is_child(p))
						vtab->pf_sum_sub(result, (char*)((p&~3) + doff));
				}
				else {
					l = p;
					p = TRB_raw_link(p,0);
				}
			}
		} while (TRB_self_is_child(p));
	}
	return (struct trb_node*)l;
}

/////////////////////////////////////////////////////////////////////////////////////////////

#define trb_compare_kd  CAT_TOKEN2(trb_compare_, FIELD_TYPE_NAME)
#define trb_find_xx CAT_TOKEN2(trb_find_, FIELD_TYPE_NAME)
#define trb_lower_bound_xx CAT_TOKEN2(trb_lower_bound_, FIELD_TYPE_NAME)
#define trb_upper_bound_xx CAT_TOKEN2(trb_upper_bound_, FIELD_TYPE_NAME)
#define trb_equal_range_xx CAT_TOKEN2(trb_equal_range_, FIELD_TYPE_NAME)
#define trb_find_path_xx  CAT_TOKEN2(trb_find_path_, FIELD_TYPE_NAME)
#define trb_get_sum_xx  CAT_TOKEN2(trb_get_sum_, FIELD_TYPE_NAME)

#define FIELD_TYPE_FILE "trb_fun_field_type.h"
#include "p_field_type_loop.h"
#undef FIELD_TYPE_FILE

/**
 @brief initialize the trb_vtab

 @note
   -# before calling this function, you must set appropriate field of vtab
        normally zero the vtab, and set \b key_offset, data_size \b
   -# vtab->data_offset == 0 indicate that only the one index
   -# for support multi-index within one node
        in this case, node_offset will be used
 */
void trb_vtab_init(struct trb_vtab* vtab, field_type_t key_type)
{
	assert(0 != vtab->data_size);
	if (0 == vtab->data_offset)
	{
		/* when only the trb_tree index, data_offset_of_whole_node can be 0,
		   in this case, here fix the data_offset
		   and, vtab->node_offset must be 0
		 */
		assert(0 == vtab->node_offset);

		vtab->data_offset = TRB_DEFAULT_DATA_OFFSET;
	}
	/*
	 * physically, data_offset can be larger than node_offfset
	 *   but, this may be wrong in most case -- caused by carelessly coding
	 *   so, only allow data_offset >= node_offset
	 */
	assert(vtab->data_offset >= vtab->node_offset);

	if (0 == vtab->node_size)
		vtab->node_size = vtab->data_offset + vtab->data_size;
	else {
		assert(vtab->node_size >= vtab->data_offset + vtab->data_size);
	}

	if (NULL == vtab->alloc)
		vtab->alloc = (struct sallocator*)mpool_get_global();

	vtab->key_type = internal_field_type(key_type);
	vtab->path_arg_size = sizeof(struct trb_path_arg);

#define TRB_FUNC_ASSIGN_0(tev, func_name) \
	vtab->pf_##func_name = trb_##func_name##_tev_##tev;

#define TRB_FUNC_ASSIGN_1(tev) \
	TRB_FUNC_ASSIGN_0(tev, lower_bound) \
	TRB_FUNC_ASSIGN_0(tev, upper_bound) \
	TRB_FUNC_ASSIGN_0(tev, equal_range)

#define TRB_FUNC_ASSIGN(tev) \
	TRB_FUNC_ASSIGN_0(tev, find_path) \
	TRB_FUNC_ASSIGN_0(tev, compare) \
	TRB_FUNC_ASSIGN_0(tev, find) \
	TRB_FUNC_ASSIGN_0(tev, get_sum) \
	TRB_FUNC_ASSIGN_1(tev)

	if (vtab->pf_compare)
	{
		// if app supplied a comparator, prefer it
		trb_func_compare trb_compare_tev_app = vtab->pf_compare;
		TRB_FUNC_ASSIGN(app);
	}
	else // use built-in comparator, can be optimized
	{
		switch (vtab->key_type)
		{
		default:
			abort();
			break;
		case tev_char:
			TRB_FUNC_ASSIGN(char);
			vtab->key_size = 1;
			break;
		case tev_byte:
			TRB_FUNC_ASSIGN(byte);
			vtab->key_size = 1;
			break;
		case tev_uint16:
			TRB_FUNC_ASSIGN(uint16);
			vtab->key_size = 2;
			break;
		case tev_int16:
			TRB_FUNC_ASSIGN(int16);
			vtab->key_size = 2;
			break;
		case tev_uint32:
			TRB_FUNC_ASSIGN(uint32);
			vtab->key_size = 4;
			break;
		case tev_int32:
			TRB_FUNC_ASSIGN(int32);
			vtab->key_size = 4;
			break;
		case tev_uint64:
			TRB_FUNC_ASSIGN(uint64);
			vtab->key_size = 8;
			break;
		case tev_int64:
			TRB_FUNC_ASSIGN(int64);
			vtab->key_size = 8;
			break;
		case tev_float:
			TRB_FUNC_ASSIGN(float);
			vtab->key_size = sizeof(float); // 4
			break;
		case tev_double:
			TRB_FUNC_ASSIGN(double);
			vtab->key_size = sizeof(double); // 8
			break;
#ifdef TERARK_C_LONG_DOUBLE_SIZE
		case tev_ldouble:
			TRB_FUNC_ASSIGN(ldouble);
			vtab->key_size = sizeof(long double);
			break;
#endif
		case tev_blob:
			{
				trb_func_compare trb_compare_tev_app = &trb_compare_tev_blob;
				assert(vtab->key_size != 0);
				TRB_FUNC_ASSIGN(app);
			}
			break;
		case tev_app:
			{
				trb_func_compare trb_compare_tev_app = vtab->pf_compare;
				TRB_FUNC_ASSIGN(app);
			}
			break;
		}
	}
}

void
trb_init(struct trb_tree* tree)
{
	tree->trb_root = 0;
	tree->trb_count = 0;
}

/* Begin rotate */

#define sadd(x,y)  (*vtab->pf_sum_add)((char*)(x) + doff, (const char*)(y) + doff)
#define atom_sub(x,y)  (*vtab->pf_sum_atom_sub)((char*)(x) + doff, (const char*)(y) + doff)
#define scopy(x,y) (*vtab->pf_sum_copy)((char*)(x) + doff, (const char*)(y) + doff)
#define sinit(x)   (*vtab->pf_sum_init)((char*)(x) + doff)
#define reCompute(x) \
  do { \
	sinit(x);		 \
	if (TAG_is_child(x, 0)) sadd(x, TRB_get_link(x, 0)); \
	if (TAG_is_child(x, 1)) sadd(x, TRB_get_link(x, 1)); \
  } while(0)

static
char*
trb_safe_sum_loop(const struct trb_vtab* vtab, struct trb_tree* tree, struct trb_node* node, int depth)
{
	TRB_define_node_offset
	char* xx;
	char* src_data = (char*)node + vtab->data_offset;
	int cmp;
	size_t dsize = TRB_GET_DATA_SIZE(vtab, src_data);

	assert(depth < TRB_MAX_HEIGHT);

	xx = (char*)malloc(dsize);

	if (vtab->pf_data_copy)
		vtab->pf_data_copy(vtab, tree, xx, src_data);
	else
		memcpy(xx, src_data, dsize);

	vtab->pf_sum_init(xx);

	if (TAG_is_child(node, 0)) {
		char* x0 = trb_safe_sum_loop(vtab, tree, TRB_get_link(node, 0), depth+1);
		vtab->pf_sum_add(xx, x0);
		free(x0);
	}
	if (TAG_is_child(node, 1)) {
		char* x1 = trb_safe_sum_loop(vtab, tree, TRB_get_link(node, 1), depth+1);
		vtab->pf_sum_add(xx, x1);
		free(x1);
	}

	cmp = vtab->pf_sum_comp(xx, (char*)node + vtab->data_offset);
//	assert(0 == cmp);
	if (0 != cmp)
		cmp = cmp;

	return xx;
}

//static
void
trb_check_sum(const struct trb_vtab* vtab, struct trb_tree* tree, struct trb_node* node)
{
	char* result = trb_safe_sum_loop(vtab, tree, node, 0);
	free(result);
}

#if defined(_DEBUG) || !defined(NDEBUG)
#  define DBG_trb_check_sum(node) trb_check_sum(vtab, (struct trb_tree*)tree, node)
#else
#  define DBG_trb_check_sum(node)
#endif

#if defined(_DEBUG) && 0
// DBG_trb_check_all is very expensive, so enable it manually
// only for trouble shooting
#  define DBG_trb_check_all() \
	if (vtab->pf_sum_add && tree->trb_root) \
		DBG_trb_check_sum(tree->trb_root); \
	else NULL
#else
#  define DBG_trb_check_all()
#endif

static
void
trb_rotate_for_insert(const struct trb_vtab* vtab,
					  volatile struct trb_tree* tree,
					  struct trb_node* n, ///< new inserted node
					  struct trb_path_arg* arg)
{
	TRB_define_node_offset
	int k = arg->height;
	int dir = TPA_dir(k-1);
	struct trb_node* p = TPA_ptr(k-1);
	int doff = vtab->data_offset;
	if (vtab->pf_sum_add) {
		int i;
		for (i = 1; i < k; ++i) sadd(TPA_ptr(i), n);
	}
	assert(arg->hlow <= k-1);
	/*
	*    p                    p
	*            ==>          |
	*                         n
	*/
	TAG_set_thread(n, 0);
	TAG_set_thread(n, 1);
	TRB_set_link(n, dir, TRB_get_link(p, dir)); // thread

	if (terark_likely(tree->trb_root != NULL)) {
		TAG_set_child(p, dir);
		TRB_set_link(n, !dir, p); // thread ptr
	} else
		TRB_set_link(n, 1, NULL); // thread ptr

	assert(TAG_is_child(p, dir));
	TRB_set_link(p, dir, n);
	TRB_set_red(n);

	while (k >= 3 && TRB_is_red(TPA_ptr(k-1)))
	{
		struct trb_node* p3 = TPA_ptr(k-3);
		struct trb_node* p2 = TPA_ptr(k-2);
		struct trb_node* p1 = TPA_ptr(k-1);

		if (TPA_dir(k-2) == 0)
		{
			/*
			*		p1 == p2->left
			*		u  == p2->right
			*/
			struct trb_node* u = TRB_get_link(p2, 1);
			if (TAG_is_child(p2, 1) && TRB_is_red(u))
			{ // p2->right != NULL && u->color == red, if ignore thread
				TRB_set_black(p1);
				TRB_set_black(u);
				TRB_set_red(p2);
				k -= 2;
			}
			else // p2->right == NULL || u->color == red, if ignore thread
			{
				struct trb_node *y;

				if (TPA_dir(k-1) == 0) // p == p1->left, if ignore thread
					y = p1;
				else
				{  //    p == p1->right
					/*
					*       p3                          p3
					*       |                           |
					*       p2                          p2
					*      /  \          ==>           /  \
					*     p1   u                      y    u
					*    /  \                        / \
					*   ?    y                      p1  Y1
					*       / \                    /  \
					*     Y0   Y1                 ?    Y0
					*/
					y = TRB_get_link(p1, 1);
					TRB_set_link(p1, 1, TRB_get_link(y, 0));
					TRB_set_link(y , 0, p1);
					TRB_set_link(p2, 0, y);

					if (TAG_is_thread(y, 0))
					{
						TAG_set_child(y, 0);
						TAG_set_thread(p1, 1);
						TRB_set_link(p1, 1, y);
					}
					if (vtab->pf_sum_add) {
					//	scopy(y, p1); // y will be set as p2 after
						reCompute(p1);
						DBG_trb_check_sum(p1);
					}
				}
				/*
				*               p3                            p3
				*               |                             |
				*               p2                            y
				*              /  \              ==>       /     \
				*             y    u                      p1      p2
				*            / \                         /  \    /  \
				*           p1  Y1                      n?  Y0  Y1   u
				*          /  \
				*         n?  Y0
				*/
				TRB_set_red(p2);
				TRB_set_black(y);

				TRB_set_link(p2, 0, TRB_get_link(y, 1));
				TRB_set_link(y, 1, p2);
				TRB_set_link(p3, TPA_dir(k-3), y);

				if (TAG_is_thread(y, 1))
				{
					TAG_set_child(y, 1);
					TAG_set_thread(p2, 0);
					TRB_set_link(p2, 0, y);
				}
				assert(TRB_get_link(p2, 1) == u);
				if (vtab->pf_sum_add) {
					scopy(y, p2); // all of p2's descendents has been y's
					reCompute(p2);
				#if defined(_DEBUG) || !defined(NDEBUG)
					if (p3 != (struct trb_node*)TRB_root_as_node(tree))
						DBG_trb_check_sum(p3);
					else
						DBG_trb_check_sum(p2);
				#endif
				}
				break;
			}
		}
		else
		{
			/*
			*	p1 == p2->right
			*	u  == p2->left
			*/
			struct trb_node* u = TRB_get_link(p2, 0);

			if (TAG_is_child(p2, 0) && TRB_is_red(u))
			{
				TRB_set_black(p1);
				TRB_set_black(u);
				TRB_set_red(p2);
				k -= 2;
			}
			else {
				struct trb_node *y;

				if (TPA_dir(k-1) == 1)
					y = p1;
				else
				{
					/*
					*       p3                                  p3
					*       |                                   |
					*       p2                                  p2
					*      /  \           ==>                  /  \
					*     u   p1                              u    y
					*        /  \                                 /  \
					*       y    ?                               Y0  p1
					*      / \                                      /  \
					*     Y0 Y1                                    Y1   ?
					*/
					y = TRB_get_link(p1, 0);
					TRB_set_link(p1, 0, TRB_get_link(y, 1));
					TRB_set_link(y , 1, p1);
					TRB_set_link(p2, 1, y);

					if (TAG_is_thread(y, 1))
					{
						TAG_set_child(y, 1);
						TAG_set_thread(p1, 0);
						TRB_set_link(p1, 0, y);
					}
					if (vtab->pf_sum_add) {
					//	scopy(y, p1); // y will be set as p2 after
						reCompute(p1);
						DBG_trb_check_sum(p1);
					}
				}
				/*
				*        p3                                       p3
				*        |                                        |
				*        p2                                     _.y\._
				*       /  \              ==>                  /      \
				*      u    y\                                p2      p1
				*          / \\                              /  \     / \
				*         Y0  p1                            u   Y0   Y1  ?
				*            /  \
				*           Y1   ?
				*/
				TRB_set_red(p2);
				TRB_set_black(y);

				TRB_set_link(p2, 1, TRB_get_link(y, 0));
				TRB_set_link(y, 0, p2);
				TRB_set_link(p3, TPA_dir(k-3), y);

				if (TAG_is_thread(y, 0))
				{
					TAG_set_child(y, 0);
					TAG_set_thread(p2, 1);
					TRB_set_link(p2, 1, y);
				}
				if (vtab->pf_sum_add) {
					scopy(y, p2); // all of p2's descendents has been y's
					reCompute(p2);
				#if defined(_DEBUG) || !defined(NDEBUG)
					if (p3 != (struct trb_node*)TRB_root_as_node(tree))
						DBG_trb_check_sum(p3);
					else
						DBG_trb_check_sum(p2);
				#endif
				}
				break;
			}
		}
	}
	TRB_set_black(tree->trb_root);
}

#if 0
#define SUM_ajust(newchild, newparent) \
	reCompute(newchild); \
	reCompute(newparent);
#else
#define SUM_ajust(newchild, newparent) \
	scopy(newparent, newchild); \
	reCompute(newchild);
#endif

static
void
trb_rotate_for_remove(const struct trb_vtab* vtab, volatile struct trb_tree* tree, struct trb_path_arg* arg)
{
	TRB_define_node_offset
	int k = arg->hlow;
	int doff = vtab->data_offset;
	struct trb_node* p = TPA_ptr(k);
	assert(k >= 1);

	if (vtab->pf_sum_add) {
		int i;
		for (i = k-1; i >= 1; --i) {
			atom_sub(TPA_ptr(i), p);
		}
	}

	if (TAG_is_thread(p, 1))
	{
		if (TAG_is_child(p, 0))
		{
			struct trb_node* t = TRB_get_link(p, 0);

			while (TAG_is_child(t, 1))
				t = TRB_get_link(t, 1);
			TRB_set_link(t, 1, TRB_get_link(p, 1)); // t.next = p.next
			TRB_set_link(TPA_ptr(k-1), TPA_dir(k-1), TRB_get_link(p, 0));
			DBG_trb_check_all();
		}
		else // p.link[0,1] are all threads, p is a leaf
		{
			TRB_set_link(TPA_ptr(k-1), TPA_dir(k-1), TRB_get_link(p, TPA_dir(k-1)));
			if (TPA_ptr(k-1) != (struct trb_node *)TRB_root_as_node(tree))
				TAG_set_thread(TPA_ptr(k-1), TPA_dir(k-1));
			DBG_trb_check_all();
		}
	}
	else // p.link[1] is child
	{
		struct trb_node* r = TRB_get_link(p, 1);

		if (TAG_is_thread(r, 0))
		{
			TRB_set_link(r, 0, TRB_get_link(p, 0));
			TAG_copy(r, 0, p, 0);
			if (TAG_is_child(p, 0))
			{
				struct trb_node* t = TRB_get_link(p, 0);
				while (TAG_is_child(t, 1))
					t = TRB_get_link(t, 1);
				TRB_set_link(t, 1, r); // thread t.next to r

				if (vtab->pf_sum_add) {
					reCompute(r);
				}
			}
			TRB_set_link(TPA_ptr(k-1), TPA_dir(k-1), r);
			TRB_swap_color(r, p);
			TPA_set(k, r, 1);
			k++;
			DBG_trb_check_all();
		}
		else // is_child(r, 0)
		{
			struct trb_node* s;
			const int j = arg->hlow;

			for (++k;;)
			{
				TPA_set(k, r, 0);
				k++;
				s = TRB_get_link(r, 0);
				if (TAG_is_thread(s, 0))
					break;

				r = s;
			}
		//	assert(TAG_is_thread(s, 0)); // always true
		//	assert(TAG_is_child (r, 0));
		//  assert(TRB_get_link (r, 0) == s);
		//  r is parent of left most of p.link[1]
			assert(TPA_ptr(j) == p);
			assert(j == arg->hlow);
			TPA_set(j, s, 1);
			if (TAG_is_child(s, 1)) {
				TRB_set_link(r, 0, TRB_get_link(s, 1));
			}
			else
			{
				assert(TRB_get_link(r, 0) == s);
			//	TRB_set_link(r, 0, s);
				TAG_set_thread(r, 0);
			}

			TRB_set_link(s, 0, TRB_get_link(p, 0));
			if (TAG_is_child(p, 0))
			{
				struct trb_node* t = TRB_get_link(p, 0);
				while (TAG_is_child(t, 1))
					t = TRB_get_link(t, 1);
				TRB_set_link(t, 1, s); // thread t.next to s

				TAG_set_child(s, 0);
			}

			TRB_set_link(s, 1, TRB_get_link(p, 1));
			TAG_set_child(s, 1);

			TRB_swap_color(s, p);
			TRB_set_link(TPA_ptr(j-1), TPA_dir(j-1), s);

			if (vtab->pf_sum_add)
			{ // must reCompute ALL effected nodes in the stack
				int i;
				for (i = k-1; i >= j-1; --i)
				{ // must in block
					reCompute(TPA_ptr(i));
				}
				DBG_trb_check_sum(s);
				DBG_trb_check_all();
			}
		}
	}

	if (TRB_is_black(p)) // 'p' will not be used in this block
	{
		for (; k > 1; k--)
		{
			if (TAG_is_child(TPA_ptr(k-1), TPA_dir(k-1)))
			{
				struct trb_node* x = TRB_get_link(TPA_ptr(k-1), TPA_dir(k-1));
				if (TRB_is_red(x))
				{
					TRB_set_black(x);
					break;
				}
			}

			if (TPA_dir(k-1) == 0)
			{
				struct trb_node* w = TRB_get_link(TPA_ptr(k-1), 1);

				if (TRB_is_red(w))
				{
					TRB_set_black(w);
					TRB_set_red(TPA_ptr(k-1));

					TRB_set_link(TPA_ptr(k-1), 1, TRB_get_link(w, 0));
					TRB_set_link(w, 0, TPA_ptr(k-1));
					TRB_set_link(TPA_ptr(k-2), TPA_dir(k-2), w);
					if (vtab->pf_sum_add) {
						SUM_ajust(TPA_ptr(k-1), w);
						DBG_trb_check_sum(TPA_ptr(k-2));
					}
					TPA_set(k, TPA_ptr(k-1), 0);
					TPA_setptr(k-1, w);
					k++;

					w = TRB_get_link(TPA_ptr(k-1), 1);
				}
				if ( (TAG_is_thread(w, 0) || TRB_is_black(TRB_get_link(w, 0))) &&
					 (TAG_is_thread(w, 1) || TRB_is_black(TRB_get_link(w, 1))) )
				{
					TRB_set_red(w);
				}
				else
				{
					if (TAG_is_thread(w, 1) || TRB_is_black(TRB_get_link(w, 1)))
					{
						struct trb_node* y = TRB_get_link(w, 0);
						TRB_set_black(y);
						TRB_set_red(w);
						TRB_set_link(w, 0, TRB_get_link(y, 1));
						TRB_set_link(y, 1, w);
						TRB_set_link(TPA_ptr(k-1), 1, y);

						if (TAG_is_thread(y, 1))
						{
							struct trb_node* z = TRB_get_link(y, 1);
							TAG_set_child(y, 1);
							TAG_set_thread(z, 0);
							TRB_set_link(z, 0, y);
						}
						if (vtab->pf_sum_add) {
							SUM_ajust(w, y);
							DBG_trb_check_sum(TPA_ptr(k-2));
						}
						w = y;
					}

					TRB_copy_color(w, TPA_ptr(k-1));
					TRB_set_black(TPA_ptr(k-1));
					TRB_set_black(TRB_get_link(w, 1));

					TRB_set_link(TPA_ptr(k-1), 1, TRB_get_link(w, 0));
					TRB_set_link(w, 0, TPA_ptr(k-1));
					TRB_set_link(TPA_ptr(k-2), TPA_dir(k-2), w);

					if (TAG_is_thread(w, 0))
					{
						TAG_set_child(w, 0);
						TAG_set_thread(TPA_ptr(k-1), 1);
						TRB_set_link(TPA_ptr(k-1), 1, w);
					}
					if (vtab->pf_sum_add) {
						SUM_ajust(TPA_ptr(k-1), w);
						DBG_trb_check_sum(TPA_ptr(k-2));
					}
					break;
				}
			}
			else
			{
				struct trb_node* w = TRB_get_link(TPA_ptr(k-1), 0);

				if (TRB_is_red(w))
				{
					TRB_set_black(w);
					TRB_set_red(TPA_ptr(k-1));

					TRB_set_link(TPA_ptr(k-1), 0, TRB_get_link(w, 1));
					TRB_set_link(w, 1, TPA_ptr(k-1));
					TRB_set_link(TPA_ptr(k-2), TPA_dir(k-2), w);
					if (vtab->pf_sum_add) {
						SUM_ajust(TPA_ptr(k-1), w);
						DBG_trb_check_sum(TPA_ptr(k-2));
					}
					TPA_set(k, TPA_ptr(k-1), 1);
					TPA_setptr(k-1, w);
					k++;

					w = TRB_get_link(TPA_ptr(k-1), 0);
				}

				if ( (TAG_is_thread(w, 0) || TRB_is_black(TRB_get_link(w, 0))) &&
					 (TAG_is_thread(w, 1) || TRB_is_black(TRB_get_link(w, 1))) )
				{
					TRB_set_red(w);
				}
				else
				{
					if (TAG_is_thread(w, 0) || TRB_is_black(TRB_get_link(w, 0)))
					{
						struct trb_node* y = TRB_get_link(w, 1);
						TRB_set_black(y);
						TRB_set_red(w);
						TRB_set_link(w, 1, TRB_get_link(y, 0));
						TRB_set_link(y, 0, w);
						TRB_set_link(TPA_ptr(k-1), 0, y);

						if (TAG_is_thread(y, 0))
						{
							struct trb_node* z = TRB_get_link(y, 0);
							TAG_set_child(y, 0);
							TAG_set_thread(z, 1);
							TRB_set_link(z, 1, y);
						}
						if (vtab->pf_sum_add) {
							SUM_ajust(w, y);
							DBG_trb_check_sum(TPA_ptr(k-2));
						}
						w = y;
					}

					TRB_copy_color(w, TPA_ptr(k-1));
					TRB_set_black(TPA_ptr(k-1));
					TRB_set_black(TRB_get_link(w, 0));

					TRB_set_link(TPA_ptr(k-1), 0, TRB_get_link(w, 1));
					TRB_set_link(w, 1, TPA_ptr(k-1));
					TRB_set_link(TPA_ptr(k-2), TPA_dir(k-2), w);
					if (TAG_is_thread(w, 1))
					{
						TAG_set_child(w, 1);
						TAG_set_thread(TPA_ptr(k-1), 0);
						TRB_set_link(TPA_ptr(k-1), 0, w);
					}
					if (vtab->pf_sum_add) {
						SUM_ajust(TPA_ptr(k-1), w);
						DBG_trb_check_sum(TPA_ptr(k-2));
					}
					break;
				}
			}
		}//for

		if (tree->trb_root != NULL)
			TRB_set_black(tree->trb_root);

		DBG_trb_check_all();
	}
}

#undef reCompute
#undef sadd
#undef scopy
#undef sinit

/* End rotate */

/* @param tree
		this param is volatile, it maybe modified through a trb_node pointer,
		but this shouldn't be exposed to the caller
 */
struct trb_node*
trb_probe(const struct trb_vtab* vtab, struct trb_tree* tree, const void* data, int* existed)
{
	struct trb_node* n; /* Newly inserted node. */
	struct sallocator* sa;
	struct trb_path_arg arg;
	size_t dsize;

	if ((*existed = vtab->pf_find_path(vtab, tree, vtab->key_offset + (char*)data, &arg)) != 0)
		return TPA_lower;

	dsize = TRB_GET_DATA_SIZE(vtab, data);
	sa = vtab->alloc;
	n = (struct trb_node*)sa->salloc(sa, vtab->data_offset + dsize);
	if (terark_unlikely(NULL == n))
		return NULL;

	if (vtab->pf_data_copy)
	{
		if (terark_unlikely(!vtab->pf_data_copy(vtab, tree, vtab->data_offset + (char*)n, data)))
		{
			sa->sfree(sa, n, vtab->data_offset + dsize);
			return NULL;
		}
	}
	else // default do memcpy
	{
		memcpy(vtab->data_offset + (char*)n, data, dsize);
	}
	tree->trb_count++;

	trb_rotate_for_insert(vtab, tree, n, &arg);

	return n;
}

/**
 @brief insert preallocated node

 @param tree
		this param is volatile, it maybe modified through a trb_node pointer,
		but this shouldn't be exposed to the caller
 @note
   -# this node must be allocated by vatab->alloc->salloc
   -# user must judge return value, if key in @param node is existed, normally need pf_free it
 @return if key in @param node is existed in the tree, return the existed node
		 else return @param node itself
 */
struct trb_node*
trb_probe_node(const struct trb_vtab* vtab, struct trb_tree* tree, struct trb_node* node)
{
	struct trb_path_arg arg;

	if (vtab->pf_find_path(vtab, tree, vtab->key_offset + vtab->data_offset + (char*)node, &arg))
		return TPA_lower;

	tree->trb_count++;
	trb_rotate_for_insert(vtab, tree, node, &arg);
	return node;
}

int trb_erase(const struct trb_vtab* vtab, struct trb_tree* tree, const void* key)
{
	struct trb_path_arg arg;
	size_t dsize;
	assert(tree != NULL && key != NULL);

	if (terark_unlikely(NULL == tree->trb_root))
		return 0;

	if (vtab->pf_find_path(vtab, tree, key, &arg))
	{
		struct trb_node* p = TPA_lower;

		tree->trb_count--;
		trb_rotate_for_remove(vtab, tree, &arg);

		dsize = TRB_GET_DATA_SIZE(vtab, vtab->data_offset + (char*)p);
		vtab->alloc->sfree(vtab->alloc, p, vtab->data_offset + dsize);
		return 1;
	}
	return 0;
}

struct trb_node*
trb_iter_first(ptrdiff_t node_offset, const struct trb_tree* tree)
{
	assert(tree != NULL);
	{
		struct trb_node* iter = tree->trb_root;
		if (terark_likely(NULL != iter))
		{
			while (TAG_is_child(iter, 0))
				iter = TRB_get_link(iter, 0);
		}
		return iter;
	}
}

/// to last node
struct trb_node*
trb_iter_last(ptrdiff_t node_offset, const struct trb_tree* tree)
{
	assert(tree != NULL);
	{
		struct trb_node* iter = tree->trb_root;
		if (terark_likely(NULL != iter))
		{
			while (TAG_is_child(iter, 1))
				iter = TRB_get_link(iter, 1);
		}
		return iter;
	}
}

struct trb_node*
trb_iter_next(ptrdiff_t node_offset, struct trb_node* iter)
{
	assert(iter != NULL);

	if (TAG_is_thread(iter, 1))
	{
		return TRB_get_link(iter, 1);
	}
	else
	{
		iter = TRB_get_link(iter, 1);
		while (TAG_is_child(iter, 0))
			iter = TRB_get_link(iter, 0);
		return iter;
	}
}

struct trb_node*
trb_iter_prev(ptrdiff_t node_offset, struct trb_node* iter)
{
	assert(iter != NULL);

	if (TAG_is_thread(iter, 0))
	{
		return TRB_get_link(iter, 0);
	}
	else
	{
		iter = TRB_get_link(iter, 0);
		while (TAG_is_child(iter, 1))
			iter = TRB_get_link(iter, 1);
		return iter;
	}
}

static
int
copy_node(const struct trb_vtab* vtab,
		  struct trb_tree* tree,
          struct trb_node* dst, int dir,
          const struct trb_node* src)
{
	TRB_define_node_offset
	size_t dsize = TRB_GET_DATA_SIZE(vtab, vtab->data_offset + (char*)src);
	struct trb_node* newp = (struct trb_node*)vtab->alloc->salloc(vtab->alloc, vtab->data_offset + dsize);

	if (newp == NULL)
		return 0;
	TRB_set_link(newp, dir, TRB_get_link(dst, dir));
	TAG_set_thread(newp, dir);
	TRB_set_link(newp, !dir, dst);
	TAG_set_thread(newp, !dir);
	TRB_set_link(dst, dir, newp);
	TAG_set_child(dst, dir);
	TRB_copy_color(newp, src);

	if (vtab->pf_data_copy)
	{
		return vtab->pf_data_copy(vtab, tree,
				vtab->data_offset + (char*)newp,
				vtab->data_offset + (char*)src
				);
	}
	else
	{
		memcpy(vtab->data_offset + (char*)newp,
			   vtab->data_offset + (char*)src, dsize);
		return 1;
	}
}

static void
copy_error_recovery(const struct trb_vtab* vtab, struct trb_node* p, struct trb_tree* newp)
{
	TRB_define_node_offset
	newp->trb_root = p;
	if (p != NULL)
	{
		while (TAG_is_child(p, 1))
			p = TRB_get_link(p, 1);
		TRB_set_link(p, 1, NULL);
	}
	trb_destroy(vtab, newp);
}

int
trb_copy(const struct trb_vtab* vtab,
		 struct trb_tree* dest,
		 const struct trb_tree* org)
{
	TRB_define_node_offset
	const struct trb_node*p;
	struct trb_node*q;
	struct trb_node rp, rq;

	assert(NULL != org);
	assert(NULL != dest);

	dest->trb_root = NULL;

	if (0 == (dest->trb_count = org->trb_count))
		return 1;

	p = TRB_revert_offset(&rp);
	TRB_set_link(p, 0, org->trb_root);
	TAG_set_child(p, 0);

	q = TRB_revert_offset(&rq);
	TRB_set_link(q, 0, NULL);
	TAG_set_thread(q, 0);

	for (;;)
	{
		if (TAG_is_child(p, 0))
		{
			if (!copy_node(vtab, dest, q, 0, TRB_get_link(p, 0)))
			{
				copy_error_recovery(vtab, TRB_get_link(TRB_revert_offset(&rq), 0), dest);
				return 0;
			}

			p = TRB_get_link(p, 0);
			q = TRB_get_link(q, 0);
		}
		else
		{
			while (TAG_is_thread(p, 1))
			{
				p = TRB_get_link(p, 1);
				if (p == NULL)
				{
					TRB_set_link(q, 1, NULL);
					dest->trb_root = TRB_get_link(TRB_revert_offset(&rq), 0);
					return 1;
				}
				q = TRB_get_link(q, 1);
			}

			p = TRB_get_link(p, 1);
			q = TRB_get_link(q, 1);
		}

		if (TAG_is_child(p, 1))
			if (!copy_node(vtab, dest, q, 1, TRB_get_link(p, 1)))
			{
				copy_error_recovery(vtab, TRB_get_link(TRB_revert_offset(&rq), 0), dest);
				return 0;
			}
	}
	return 1;
}

void
trb_destroy(const struct trb_vtab* vtab, struct trb_tree* tree)
{
	TRB_define_node_offset
	struct trb_node*p; /* Current node. */
	struct trb_node*n; /* Next node. */

	p = tree->trb_root;
	if (terark_likely(NULL != p))
		while (TAG_is_child(p, 0))
			p = TRB_get_link(p, 0);

	while (p != NULL)
	{
		void*  pdata = vtab->data_offset + (char*)p;
		size_t dsize = TRB_GET_DATA_SIZE(vtab, pdata);

		n = TRB_get_link(p, 1);
		if (TAG_is_child(p, 1))
			while (TAG_is_child(n, 0))
				n = TRB_get_link(n, 0);

		if (vtab->pf_data_destroy)
			vtab->pf_data_destroy(vtab, tree, pdata);

		vtab->alloc->sfree(vtab->alloc, p, vtab->data_offset + dsize);

		p = n;
	}

	tree->trb_root = NULL;
	tree->trb_count = 0;
}


