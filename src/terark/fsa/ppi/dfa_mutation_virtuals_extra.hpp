public:
// dead states: states which are unreachable to any final states
template<class SrcDFA>
void remove_dead_states(const SrcDFA& src) {
	typedef typename SrcDFA::state_id_t SrcID;
	assert(src.has_freelist());
	CompactReverseNFA<SrcID> inv(src.graph_reverse_nfa());
	valvec<SrcID> stack;
	febitvec mark(src.total_states(), false);
	if (src.num_free_states()) {
		for (size_t root = 0, n = src.total_states(); root < n; ++root) {
			if (!src.is_free(root) && src.is_term(root))
				stack.push_back(root), mark.set1(root);
		}
	} else { // has no any free state
		for (size_t root = 0, n = src.total_states(); root < n; ++root) {
			if (src.is_term(root))
				stack.push_back(root), mark.set1(root);
		}
	}
	// PFS search from multiple final states
	while (!stack.empty()) {
		size_t parent = stack.back(); stack.pop_back();
		size_t beg = inv.index[parent+0];
		size_t end = inv.index[parent+1];
		assert(beg <= end);
		for(size_t i = beg; i < end; ++i) {
			size_t child = inv.data[i].target;
			assert(child < src.total_states());
			if (mark.is0(child))
				stack.push_back(child), mark.set1(child);
		}
	}
	size_t new_au_states = 0;
	valvec<state_id_t> map(src.total_states(), (state_id_t)nil_state);
	for (size_t s = 0, n = src.total_states(); s < n; ++s) {
		if (mark.is1(s))
			map[s] = new_au_states++;
	}
	this->erase_all();
	this->resize_states(new_au_states);
	terark::AutoFree<CharTarget<size_t> > children(src.get_sigma());
	for(size_t parent = 0, n = src.total_states(); parent < n; ++parent) {
		size_t parent2 = map[parent];
		if (parent2 == nil_state)
			continue;
		size_t k = 0;
		src.for_each_move(parent, [&](SrcID child, auchar_t ch) {
			size_t child2 = map[child];
			if (child2 != nil_state)
				children[k++] = CharTarget<size_t>(ch, child2);
		});
		this->add_all_move(parent2, children, k);
		if (src.is_term(parent))
			this->set_term_bit(parent2);
	}
	this->shrink_to_fit();
}

// sub graph rooted at RootState must has no intersection nodes
// with other sub graphs
void remove_sub(size_t RootState) {
	assert(RootState < total_states());
	auto on_finish = [this](state_id_t s) { del_state(s); };
	auto on_back_edge = [&](state_id_t, state_id_t child) {
		if (child == RootState) {
			string_appender<> msg;
			msg << BOOST_CURRENT_FUNCTION
				<< ": found cycle to RootState=" << RootState;
			throw std::logic_error(msg);
		}
	};
	dfs_post_order_walk(RootState, on_finish, on_back_edge);
}

///@param on_fold:
//        void on_fold.start(state_id_t RootState);
///       void on_fold.on_value(size_t nth, fstring value);
///       bool on_fold.finish(size_t num_values, std::string* folded_value);
/// values are suffixes of prefixes after first delim
///@effect
///      if on_fold.finish returns true, values will be
///      folded into a single value (such as sum of numbers)
///@note this Automata must has no cycles in values
template<class OnFold>
void fold_values(auchar_t delim, OnFold* on_fold) {
	fold_values<OnFold&>(delim, *on_fold);
}
template<class OnFold>
void fold_values(auchar_t delim, OnFold on_fold) {
	assert(num_zpath_states() == 0);
	if (this->m_zpath_states) {
		THROW_STD(invalid_argument, "DFA must not be path_zip'ed");
	}
	valvec<state_id_t> folds; folds.reserve(128);
	{
	//	BFS_MultiPassGraphWalker<state_id_t> walker; // BFS use more memory
	//	DFS_MultiPassGraphWalker<state_id_t> walker;
		DFS_GraphWalker<state_id_t> walker;
		febitvec in_folds(total_states(), 0);
		walker.resize(total_states());
		walker.putRoot(initial_state);
		while (!walker.is_finished()) {
			state_id_t parent = walker.next();
			state_id_t vchild = state_move(parent, delim);
			if (nil_state != vchild && in_folds.is0(vchild)) {
				in_folds.set1(vchild);
				folds.push_back(vchild);
			}
			walker.putChildren2(this, parent,
				[=](state_id_t, state_id_t child, bool) {
					// just fold at first delim bound
					return (child != vchild);
				});
		}
	#if !defined(NDEBUG)
		size_t n_reachable = 0;
		walker.resize(total_states());
		walker.putRoot(initial_state);
		while (!walker.is_finished()) {
			state_id_t parent = walker.next();
			walker.putChildren(this, parent);
			n_reachable++;
		}
		assert(num_used_states() == n_reachable);
	#endif
	}
	MatchContext ctx;
	valvec<state_id_t> indegree(total_states(), 0);
	compute_indegree(initial_state, indegree);
	auto on_suffix = [&](size_t nth, fstring value, size_t) {
		on_fold.on_value(nth, value);
	};
	NonRecursiveForEachWord forEachWord(&ctx);
	std::string folded_value;
	valvec<state_id_t> del_list;
	valvec<state_id_t> topological, stack, color;
	for (size_t j = 0; j < folds.size(); ++j) {
		state_id_t s = folds[j];
	#if !defined(NDEBUG)
		for (size_t i = 1; i < indegree.size(); ++i) {
			if (is_free(i)) continue;
			// if algorithm is correct, indegree[i] must > 0
			assert(indegree[i] > 0);
		}
	#endif
		on_fold.start(s);
		size_t nth = forEachWord(*this, s, 0, on_suffix); // will call on_fold.on_value(...)
		if (on_fold.finish(nth, &folded_value)) {
			del_list.erase_all();
			topological.erase_all();
			assert(stack.empty());
			color.resize(total_states()+1);
			stack.push_back(s);
			this->topological_sort(stack, color,
					[&](state_id_t x) { topological.push_back(x); });
			assert(topological.back() == s);
			topological.pop_back(); // pop s
			for_each_dest(s, [&](state_id_t child) {
					if (0 == --indegree[child])
						del_list.push_back(child);
				});
			for (ptrdiff_t i = topological.size()-1; i >= 0; --i) {
				state_id_t parent = topological[i];
				for_each_dest(parent,
					[&,parent](state_id_t child) {
						if (0 == indegree[parent]) {
							if (0 == --indegree[child])
								del_list.push_back(child);
						}
					});
			}
			for (state_id_t x : del_list) this->del_state(x);
			this->del_all_move(s);
			fstring folded(folded_value);
			state_id_t curr = s;
			indegree.resize(std::max(total_states(), num_used_states()+folded.n));
			for (ptrdiff_t i = 0; i < folded.n; ++i) {
				state_id_t next = new_state();
				add_move(curr, next, (byte_t)folded.p[i]);
				indegree[next] = 1;
				curr = next;
			}
			set_term_bit(curr);
			assert(indegree.size() == total_states());
		}
	}
#if !defined(NDEBUG)
	for (size_t i = 1; i < indegree.size(); ++i) {
		if (is_free(i)) continue;
		// if algorithm is correct, indegree[i] must > 0
		assert(indegree[i] > 0);
	}
	// check new indegrees
	indegree.resize_no_init(total_states());
	indegree.fill(0);
	for (size_t i = 0; i < indegree.size(); ++i) {
		if (is_free(i)) continue;
		for_each_dest(i, [&](state_id_t child) {
				indegree[child]++;
			});
	}
	for (size_t i = 1; i < indegree.size(); ++i) {
		if (is_free(i)) continue;
		// if algorithm is correct, indegree[i] must > 0
		assert(indegree[i] > 0);
	}
#endif
}

