public:
template<class NFA>
void compute_reverse_nfa(NFA* nfa) const {
	compute_reverse_nfa(initial_state, nfa);
}
template<class NFA>
void compute_reverse_nfa(state_id_t MyRoot, NFA* nfa) const {
	valvec<state_id_t> stack;
	febitvec       color(total_states());
	stack.reserve(std::min<size_t>(64, this->total_states()));
	stack.push_back(MyRoot);
	color.set1(MyRoot);
	nfa->erase_all();
	nfa->resize_states(total_states()+1);
	while (!stack.empty()) {
		state_id_t parent = stack.back(); stack.pop_back();
		for_each_move(parent,
			[&,parent](state_id_t child, auchar_t c) {
				assert(c < NFA::sigma);
				if (color.is0(child)) {
					color.set1(child);
					stack.push_back(child);
				}
				nfa->push_move(child+1, parent+1, c);
			});
		if (is_term(parent)) {
			nfa->push_epsilon(initial_state, parent+1);
		}
	}
	nfa->set_final(initial_state + 1);
	nfa->sort_unique_shrink_all();
}
void compute_reverse_nfa(CompactReverseNFA<state_id_t>* nfa) const {
	compute_reverse_nfa(initial_state, nfa);
}
void compute_reverse_nfa(state_id_t MyRoot, CompactReverseNFA<state_id_t>* nfa)
const {
	const bool HasFreeStates = (0 != num_free_states());
	nfa->index.resize(total_states() + 2); // the extra element is a guard
	// reverse_nfa will add an extra state for link initial_state of reverse nfa
	// with the final states of original dfa
	// reverse_nfa_state_id <==> original_dfa_state_id + 1
	for (size_t s = 0; s < total_states(); ++s)
	  if (!(HasFreeStates && is_free(s)))
		for_each_dest(s, [&](state_id_t t) {
			nfa->index[t+2]++;
		});
	// non-epsilon move number of reverse_nfa's intial_state is zero
	assert(0 == nfa->index[0]);
	assert(0 == nfa->index[1]);
	for (size_t i = 2, n = nfa->index.size(); i < n; ++i)
		nfa->index[i] += nfa->index[i-1];
	nfa->data.resize_no_init(nfa->index.back());
	valvec<state_id_t> ii = nfa->index;
	for (size_t s = 0; s < total_states(); ++s)
	  if (!(HasFreeStates && is_free(s))) {
		for_each_move(s, [&,s](state_id_t t, auchar_t c) {
			nfa->data[ii[t+1]++] = CharTarget<state_id_t>(c, s+1);
		});
		if (is_term(s)) {
			nfa->push_epsilon(initial_state, s+1);
		}
	  }
	nfa->set_final(initial_state + 1);
	nfa->sort_unique_shrink_all();
}


