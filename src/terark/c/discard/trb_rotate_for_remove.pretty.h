static
void
trb_rotate_for_remove(const struct trb_vtab* terark_restrict vtab, volatile struct trb_tree* tree, struct trb_path_arg* terark_restrict arg)
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
	if (TAG_is_thread(p, 1)) {
		if (TAG_is_child(p, 0)) {
			struct trb_node* t = TRB_get_link(p, 0);
			while (TAG_is_child(t, 1))
				t = TRB_get_link(t, 1);
			TRB_set_link(t, 1, TRB_get_link(p, 1)); // t.next = p.next
			TRB_set_link(TPA_ptr(k-1), TPA_dir(k-1), TRB_get_link(p, 0));
			DBG_trb_check_all();
		}
		else { // p.link[0,1] are all threads, p is a leaf
			TRB_set_link(TPA_ptr(k-1), TPA_dir(k-1), TRB_get_link(p, TPA_dir(k-1)));
			if (TPA_ptr(k-1) != (struct trb_node *)TRB_root_as_node(tree))
				TAG_set_thread(TPA_ptr(k-1), TPA_dir(k-1));
			DBG_trb_check_all();
		}
	}
	else // p.link[1] is child
	{
		struct trb_node* r = TRB_get_link(p, 1);
		if (TAG_is_thread(r, 0)) {
			TRB_set_link(r, 0, TRB_get_link(p, 0));
			TAG_copy(r, 0, p, 0);
			if (TAG_is_child(p, 0)) {
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
		else { // is_child(r, 0)
			struct trb_node* s;
			const int j = arg->hlow;
			for (++k;;) {
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
			TPA_set(j, s, 1);
			if (TAG_is_child(s, 1)) {
				TRB_set_link(r, 0, TRB_get_link(s, 1));
			} else {
				assert(TRB_get_link(r, 0) == s);
			//	TRB_set_link(r, 0, s);
				TAG_set_thread(r, 0);
			}
			TRB_set_link(s, 0, TRB_get_link(p, 0));
			if (TAG_is_child(p, 0)) {
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
			if (TAG_is_child(TPA_ptr(k-1), TPA_dir(k-1))) {
			  struct trb_node* x =
				TRB_get_link(TPA_ptr(k-1), TPA_dir(k-1));
				if (TRB_is_red(x)) {
					TRB_set_black(x);
					break;
				}
			}
			if (TPA_dir(k-1) == 0) {
				struct trb_node* w = TRB_get_link(TPA_ptr(k-1), 1);
				if (TRB_is_red(w)) {
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
					TRB_set_red(w);
				else {
					if (TAG_is_thread(w, 1) || TRB_is_black(TRB_get_link(w, 1))) {
						struct trb_node* y = TRB_get_link(w, 0);
						TRB_set_black(y);
						TRB_set_red(w);
						TRB_set_link(w, 0, TRB_get_link(y, 1));
						TRB_set_link(y, 1, w);
						TRB_set_link(TPA_ptr(k-1), 1, y);
						if (TAG_is_thread(y, 1)) {
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
					if (TAG_is_thread(w, 0)) {
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
			} else {
				struct trb_node* w = TRB_get_link(TPA_ptr(k-1), 0);
				if (TRB_is_red(w)) {
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
					TRB_set_red(w);
				else {
					if (TAG_is_thread(w, 0) || TRB_is_black(TRB_get_link(w, 0))) {
						struct trb_node* y = TRB_get_link(w, 1);
						TRB_set_black(y);
						TRB_set_red(w);
						TRB_set_link(w, 1, TRB_get_link(y, 0));
						TRB_set_link(y, 0, w);
						TRB_set_link(TPA_ptr(k-1), 0, y);
						if (TAG_is_thread(y, 0)) {
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
					if (TAG_is_thread(w, 1)) {
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
