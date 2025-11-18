
// No mark bits, optimized for tree shape Automata
template<class Visitor>
void bfs_tree(valvec<state_id_t> q[2], Visitor vis) const {
	assert(q[1].size() == 0);
	do {
		for (size_t i = 0; i < q[0].size(); ++i) {
			state_id_t parent = q[0][i];
			for_each_move(parent,
				[&,parent](state_id_t child, auchar_t ch) {
					vis(parent, child, ch);
					q[1].push_back(child);
				});
		}
		q[0].swap(q[1]);
		q[1].erase_all();
	} while (!q[0].empty());
}
template<class Visitor>
void bfs_tree(valvec<state_id_t> q[2], Visitor* vis) const {
	bfs_tree<Visitor&>(q, *vis);
}

template<class Visitor>
void bfs_tree(state_id_t RootState, Visitor vis) const {
	valvec<state_id_t> q[2];
	q[0].push_back(RootState);
	bfs_tree<Visitor>(q, vis);
}
template<class Visitor>
void bfs_tree(state_id_t RootState, Visitor* vis) const {
	bfs_tree<Visitor&>(RootState, *vis);
}

/*
template<class Visitor>
void bfs_tree_children(state_id_t RootState, Visitor vis) const {
	valvec<state_id_t> q[2];
	q[0].resize_no_init(this->m_dyn_sigma);
	size_t cnt = this->get_all_move(RootState, &q[0]);
	q[0].risk_set_size(cnt);
	bfs_tree<Visitor>(q, vis);
}
template<class Visitor>
void bfs_tree_children(state_id_t RootState, Visitor* vis) const {
	bfs_tree_children<Visitor&>(RootState, *vis);
}
*/

bool has_bytes(fstring bytes) {
	static_bitmap<MyType::sigma> bits(0);
	for (size_t i = 0; i < bytes.size(); ++i) {
		bits.set1((byte_t)bytes[i]);
	}
	valvec<state_id_t> stack;
	febitvec       color(total_states());
	stack.push_back(initial_state);
	color.set1(initial_state);
	while (!stack.empty()) {
		state_id_t parent = stack.back(); stack.pop_back();
		bool found = false;
		for_each_move(parent,
			[&](state_id_t child, auchar_t ch) {
				if (color.is0(child)) {
					color.set1(child);
					stack.push_back(child);
				}
				if (bits.is1(ch))
					found = true;
			});
		if (found)
			return true;
	}
	return false;
}

void compute_indegree(size_t RootState, valvec<state_id_t>& in) const {
	compute_indegree(&RootState, 1, in);
}
void
compute_indegree(const size_t* pRoots, size_t nRoots, valvec<state_id_t>& in)
const {
	TERARK_VERIFY_GE(nRoots, 1);
	TERARK_VERIFY_LE(nRoots, total_states());
	in.resize(total_states(), 0);
	febitvec color(total_states(), 0);
	size_t initial_cap = nRoots + this->get_sigma();
	valvec<state_id_t> stack(initial_cap, valvec_reserve());
	for(size_t i = 0; i < nRoots; ++i) {
		size_t root = pRoots[i];
		assert(root < total_states());
		ASSERT_isNotFree(root);
		stack.unchecked_push_back(root);
		color.set1(root);
	}
	while (!stack.empty()) {
		state_id_t parent = stack.back(); stack.pop_back();
		for_each_dest_rev(parent, [&](state_id_t child) {
			ASSERT_isNotFree(child);
			if (color.is0(child)) {
				color.set1(child);
				stack.push_back(child);
			}
			in[child]++;
		});
	}
}

void get_final_states(valvec<state_id_t>* final_states) const {
	return get_final_states(initial_state, final_states);
}
void get_final_states(state_id_t root, valvec<state_id_t>* final_states)
const {
	PFS_GraphWalker<state_id_t> walker;
	get_final_states(root, final_states, walker);
}
template<class Walker>
void get_final_states(state_id_t root, valvec<state_id_t>* final_states,
					  Walker& walker)
const {
	final_states->erase_all();
	walker.resize(total_states());
	walker.putRoot(root);
	while (!walker.is_finished()) {
		state_id_t curr = walker.next();
		if (is_term(curr))
			final_states->push_back(curr);
		walker.putChildren(this, curr);
	}
}


