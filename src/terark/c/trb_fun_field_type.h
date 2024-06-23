// input MACRO params:
//   FIELD_TYPE_TYPE
//   FIELD_TYPE_NAME
//   FIELD_TYPE_PROMOTED [optional]

// #ifndef FIELD_TYPE_PROMOTED
// #  define FIELD_TYPE_PROMOTED FIELD_TYPE_TYPE
// #endif

// use FIELD_TYPE_TYPE maybe more faster
//#undef FIELD_TYPE_PROMOTED
//#define FIELD_TYPE_PROMOTED FIELD_TYPE_TYPE

int trb_compare_kd (const struct trb_vtab*  vtab,
					const struct trb_tree*  tree, ///< state info of compare hppoled in trb_tree derived, if needed
					const void*  px,
					const void*  py)
{
	FIELD_TYPE_TYPE x = *(FIELD_TYPE_TYPE*)px;
	FIELD_TYPE_TYPE y = *(FIELD_TYPE_TYPE*)py;
	if (x < y) return -1;
	if (y < x) return +1;
	return 0;
}

#ifndef _TRB_NO_COMPRESS_NODE

struct trb_node*
trb_lower_bound_xx(const struct trb_vtab*  vtab, const struct trb_tree*  tree, const void*  key)
{
	intptr_t l = 0;
	assert(tree != NULL && key != NULL);
	if (terark_likely(NULL != tree->trb_root))
	{
		TRB_define_node_offset
		FIELD_TYPE_TYPE x = *(FIELD_TYPE_TYPE*)key;
		ptrdiff_t key_offset_of_node = vtab->data_offset + vtab->key_offset;
		intptr_t p = (intptr_t)tree->trb_root;
		do {
			p &= ~3;
			{
				FIELD_TYPE_TYPE y = *(FIELD_TYPE_TYPE*)(p + key_offset_of_node);
				if (y < x)
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
trb_upper_bound_xx(const struct trb_vtab*  vtab, const struct trb_tree*  tree, const void*  key)
{
	intptr_t u = 0;
	assert(tree != NULL && key != NULL);
	if (terark_likely(NULL != tree->trb_root))
	{
		TRB_define_node_offset
		FIELD_TYPE_TYPE x = *(FIELD_TYPE_TYPE*)key;
		ptrdiff_t key_offset_of_node = vtab->data_offset + vtab->key_offset;
		intptr_t p = (intptr_t)tree->trb_root;
		do {
			p &= ~3;
			{
				FIELD_TYPE_TYPE y = *(FIELD_TYPE_TYPE*)(p + key_offset_of_node);
				if (x < y) {
					u = p;
					p = TRB_raw_link(p,0);
				} else
					p = TRB_raw_link(p,1);
			}
		} while (TRB_self_is_child(p));
	}
	return (struct trb_node*)u;
}

// minor diff with trb_equal_range_tev_app, this compare overhead is low
// so just equal to inline expand of lower_bound+upper_bound
struct trb_node*
trb_equal_range_xx(const struct trb_vtab*  vtab,
				   const struct trb_tree*  tree,
				   const void*  key,
				   struct trb_node**  upp)
{
	intptr_t l = 0, u = 0;
	if (terark_likely(NULL != tree->trb_root))
	{
		TRB_define_node_offset
		FIELD_TYPE_TYPE x = *(FIELD_TYPE_TYPE*)key;
		ptrdiff_t key_offset_of_node = vtab->data_offset + vtab->key_offset;
		intptr_t p = (intptr_t)tree->trb_root;
		do {
			p &= ~3;
			{
				FIELD_TYPE_TYPE y = *(FIELD_TYPE_TYPE*)(p + key_offset_of_node);
				if (y < x)
					p = TRB_raw_link(p,1);
				else {
					l = p;
					p = TRB_raw_link(p,0);
				}
			}
		} while (TRB_self_is_child(p));
		p = (intptr_t)tree->trb_root;
		do {
			p &= ~3;
			{
				FIELD_TYPE_TYPE y = *(FIELD_TYPE_TYPE*)(p + key_offset_of_node);
				if (x < y) {
					u = p;
					p = TRB_raw_link(p,0);
				} else
					p = TRB_raw_link(p,1);
			}
		} while (TRB_self_is_child(p));
	}
	*upp = (struct trb_node*)(u);
	return (struct trb_node*)(l);
}

struct trb_node*
trb_find_xx(const struct trb_vtab*  vtab,
			const struct trb_tree*  tree,
			const void*  key)
{
	intptr_t l = 0;
	assert(tree != NULL && key != NULL);
	if (terark_likely(NULL != tree->trb_root))
	{
		TRB_define_node_offset
		ptrdiff_t key_offset_of_node = vtab->data_offset + vtab->key_offset;
		intptr_t p = (intptr_t)tree->trb_root;
		FIELD_TYPE_TYPE x = *(FIELD_TYPE_TYPE*)key;
		do {
			p &= ~3;
			{
				FIELD_TYPE_TYPE y = *(FIELD_TYPE_TYPE*)(p + key_offset_of_node);
				if (y < x)
					p = TRB_raw_link(p,1);
				else {
					l = p;
					p = TRB_raw_link(p,0);
				}
			}
		} while (TRB_self_is_child(p));

		// only these lines are different with trb_lower_bound_xx
		if (l && x == *(FIELD_TYPE_TYPE*)(l + key_offset_of_node))
			return (struct trb_node*)l;
	}
	return NULL;
}

#endif // _TRB_NO_COMPRESS_NODE

int
trb_find_path_xx(const struct trb_vtab*  vtab,
				 const struct trb_tree*  tree,
				 const void*  key,
				 struct trb_path_arg*  arg)
{
	int existed = 0;
	int k = 1;
	TRB_define_node_offset
	assert(tree != NULL && key != NULL);

	arg->hlow = 0; // k-1
	TPA_set(0, TRB_root_as_node(tree), 0);

	if (terark_likely(NULL != tree->trb_root))
	{
		FIELD_TYPE_TYPE x = *(FIELD_TYPE_TYPE*)key;
		ptrdiff_t key_offset_of_node = vtab->data_offset + vtab->key_offset;
		intptr_t p = (intptr_t)tree->trb_root;
		intptr_t l = 0;
		do {
			p &= ~3;
			{
				FIELD_TYPE_TYPE y = *(FIELD_TYPE_TYPE*)(p + key_offset_of_node);
				if (y < x) {
					TPA_set(k, p, 1);
					p = TRB_raw_link(p,1);
				}
				else {
					l = p;
					TPA_set(k, p, 0);
					arg->hlow = k;
					p = TRB_raw_link(p,0);
				}
				++k;
			}
		} while (TRB_self_is_child(p));

		existed = (l && x == *(FIELD_TYPE_TYPE*)(l + key_offset_of_node));
	}
	assert(arg->hlow <= k-1);
	arg->height = k;

	return existed;
}

struct trb_node*
trb_get_sum_xx(const struct trb_vtab* vtab,
				const struct trb_tree* tree,
				const void* key,
				void* result)
{
	intptr_t l = 0;
	assert(tree != NULL && key != NULL);
	if (terark_likely(NULL != tree->trb_root))
	{
		FIELD_TYPE_TYPE x = *(FIELD_TYPE_TYPE*)key;
		TRB_define_node_offset
		ptrdiff_t key_offset_of_node = vtab->data_offset + vtab->key_offset;
		ptrdiff_t doff = vtab->data_offset;
		intptr_t p = (intptr_t)tree->trb_root;
		do {
			p &= ~3;
			{
				FIELD_TYPE_TYPE y = *(FIELD_TYPE_TYPE*)(p + key_offset_of_node);
				if (x > y) {
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

#undef FIELD_TYPE_TYPE
//#undef FIELD_TYPE_PROMOTED
#undef FIELD_TYPE_NAME

