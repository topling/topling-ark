public:
valvec<CharTarget<state_id_t> > trie_reverse_dfa() const {
#if !defined(NDEBUG)
	febitvec color(total_states());
	color.set1(initial_state);
#endif
	const  bool HasFreeStates = (0 != num_free_states());
	valvec<CharTarget<state_id_t> > inv(total_states());
	for (state_id_t s = 0; s < total_states(); ++s)
	  if (!(HasFreeStates && is_free(s)))
		for_each_move(s, [&,s](state_id_t t, auchar_t c) {
			auto& ct = inv[t];
		#if !defined(NDEBUG)
			assert(color.is0(t));
			color.set1(t);
		#endif
			ct.ch = c;
			ct.target = s;
		});
	return inv;
}

// optimized
//
// create the inverse nfa, inverse of a general dfa is a nfa
//
// index[i] to index[i+1] are the index range of 'data'
//   for target set of state 'i' in the inverse nfa
CompactReverseNFA<state_id_t> graph_reverse_nfa() const {
	const bool HasFreeStates = (0 != num_free_states());
	CompactReverseNFA<state_id_t> nfa;
	nfa.index.resize(total_states() + 1); // the extra element is a guard
	for (size_t s = 0; s < total_states(); ++s)
	  if (!(HasFreeStates && is_free(s)))
		for_each_dest(s, [&](state_id_t t) {
			nfa.index[t+1]++;
		});
	for (size_t i = 1; i < nfa.index.size(); ++i)
		nfa.index[i] += nfa.index[i-1];
	nfa.data.resize_no_init(nfa.index.back());
	valvec<state_id_t> ii = nfa.index;
	for (size_t s = 0; s < total_states(); ++s)
	  if (!(HasFreeStates && is_free(s)))
		for_each_move(s, [&,s](state_id_t t, auchar_t c) {
			nfa.data[ii[t]++] = CharTarget<state_id_t>(c, s);
		});
	return nfa;
}

protected:
typedef smallmap<typename Hopcroft<state_id_t>::Splitter> ch_to_invset_t;

void hopcroft_graph_dfa_no_collapse(Hopcroft<state_id_t>& H)
const {
	assert(total_states() < (size_t)nil_state);
	CompactReverseNFA<state_id_t> inv(graph_reverse_nfa());
	H.refine([&inv](state_id_t curr, ch_to_invset_t& mE) {
		size_t low = inv.index[curr+0];
		size_t upp = inv.index[curr+1];
		for (size_t i = low; i < upp; ++i) {
			CharTarget<state_id_t>  ct = inv.data[i];
			mE.bykey(ct.ch).insert(ct.target);
		}
	});
}

public:
template<class Au2>
void
hopcroft_multi_root_dfa(const valvec<size_t>& srcRoots,
			  Au2& minimized, valvec<size_t>* dstRoots) const {
	dstRoots->resize_no_init(srcRoots.size());
	hopcroft_multi_root_dfa(srcRoots.data(), srcRoots.size(),
				 minimized, dstRoots->data());
}

template<class Au2>
void
hopcroft_multi_root_dfa(const size_t* srcRoots, size_t nRoots,
              Au2& minimized, size_t* dstRoots)
const {
	minimized.erase_all();
	assert(total_states() < (size_t)nil_state);
	Hopcroft<state_id_t> H = HopcroftUseHash()
		? Hopcroft<state_id_t>(*this, srcRoots, nRoots, HopcroftDeltaHash<MyType>())
		: Hopcroft<state_id_t>(*this, srcRoots, nRoots);
	hopcroft_graph_dfa_no_collapse(H);
	for(size_t i = 0; i < nRoots; ++i) {
		size_t src = srcRoots[i];
		assert(src < total_states());
		size_t dst = H.L[src].blid;
		assert(dst < H.P.size());
		dstRoots[i] = dst;
	}
	collapse(H, minimized);
}

template<class Au2>
void graph_dfa_minimize(Au2& minimized) const {
	minimized.erase_all();
	assert(total_states() < (size_t)nil_state);
	Hopcroft<state_id_t> H = HopcroftUseHash()
		? Hopcroft<state_id_t>(*this, HopcroftDeltaHash<MyType>())
		: Hopcroft<state_id_t>(*this);
	hopcroft_graph_dfa_no_collapse(H);
	collapse(H, minimized);
}
template<class Au2>
void trie_dfa_minimize(Au2& minimized) const {
	minimized.erase_all();
	assert(total_states() < (size_t)nil_state);
	Hopcroft<state_id_t> H = HopcroftUseHash()
		? Hopcroft<state_id_t>(*this, HopcroftDeltaHash<MyType>())
		: Hopcroft<state_id_t>(*this);
	{
		valvec<CharTarget<state_id_t> > inv(trie_reverse_dfa());
		H.refine([&inv](state_id_t curr, ch_to_invset_t& mE) {
			if (initial_state != curr) {
				auto ct = inv[curr];
				mE.bykey(ct.ch).insert(ct.target);
			}
		});
	}
	collapse(H, minimized);
}
template<class Au2>
void collapse0(Hopcroft<state_id_t>& H, Au2& minimized) const
{
	// this function has redundant calls to add_move
	size_t min_size = H.P.size();
	H.P.clear(); // H.P is no longer used
	minimized.resize_states(min_size);
	const bool HasFreeStates = (0 != num_free_states());
	for (state_id_t s = 0; s < total_states(); ++s) {
		if (HasFreeStates && is_free(s)) continue;
		for_each_move(s, [&,s](state_id_t t, auchar_t c) {
			assert(c < Au2::sigma);
			const typename Au2::state_id_t old =
				minimized.add_move(H.L[s].blid, H.L[t].blid, c);
			assert(Au2::nil_state == old || old == H.L[t].blid);
		});
		if (is_term(s))
			minimized.set_term_bit(H.L[s].blid);
	}
}
template<class Au2>
void collapse1(Hopcroft<state_id_t>& H, Au2& minimized) const {
	// this function use a bitvec mark to avoid redundant add_move
	size_t min_size = H.P.size();
	H.P.clear(); // H.P is no longer used
	minimized.resize_states(min_size);
	const bool HasFreeStates = (0 != num_free_states());
	febitvec mark(total_states(), 0);
	for (state_id_t s = 0; s < total_states(); ++s) {
		if (HasFreeStates && is_free(s)) continue;
		if (mark.is1(s)) continue;
		state_id_t curr = s;
	//	size_t cnt = 0;
		do { // set all equivalent states as marked
			assert(mark.is0(curr));
			mark.set1(curr);
			curr = H.L[curr].next;
	//		assert(cnt++ < total_states());
		} while (s != curr);
		for_each_move(s, [&,s](state_id_t t, auchar_t c) {
			auto old = minimized.add_move(H.L[s].blid, H.L[t].blid, c);
			assert(Au2::nil_state == old); TERARK_UNUSED_VAR(old);
		});
		if (is_term(s))
			minimized.set_term_bit(H.L[s].blid);
	}
}
template<class Au2>
void collapse2(Hopcroft<state_id_t>& H, Au2& minimized) const {
	// this function use H.L[*].next as mark to avoid redundant add_move
	size_t min_size = H.P.size();
	H.P.clear(); // H.P is no longer used
	minimized.resize_states(min_size);
	const bool HasFreeStates = (0 != num_free_states());
	for (state_id_t s = 0; s < total_states(); ++s) {
		if (HasFreeStates && is_free(s)) continue;
		if (nil_state == H.L[s].next) continue;
		for_each_move(s, [&,s](state_id_t t, auchar_t c) {
			auto old = minimized.add_move(H.L[s].blid, H.L[t].blid, c);
			assert(Au2::nil_state == old); TERARK_UNUSED_VAR(old);
		});
		if (is_term(s))
			minimized.set_term_bit(H.L[s].blid);
		state_id_t curr = s;
		do { // set all equivalent states as marked: L[*].next = null
			assert(nil_state != curr);
			state_id_t next = H.L[curr].next;
			H.L[curr].next = nil_state;
			curr = next;
		} while (s != curr);
	}
}
template<class Au2>
void collapse3(Hopcroft<state_id_t>& H, Au2& minimized) const {
	assert(H.L.size() == total_states());
	size_t min_size = H.P.size();
	size_t cur_size = total_states();
	terark::AutoFree<state_id_t> X, M;
	X.p = (state_id_t*)H.P.risk_release_ownership();
	M.p = (state_id_t*)H.L.risk_release_ownership();
	typedef typename Hopcroft<state_id_t>::Elem  H_Elem;
	typedef typename Hopcroft<state_id_t>::Block H_Block;
	for (size_t i = 0; i < min_size; ++i) X.p[i] = ((H_Block*)X.p)[i].head;
	for (size_t i = 0; i < cur_size; ++i) M.p[i] = ((H_Elem *)M.p)[i].blid;
	X.p = (state_id_t*)realloc(X.p, sizeof(state_id_t) * min_size);
	M.p = (state_id_t*)realloc(M.p, sizeof(state_id_t) * cur_size);
	if (NULL == X.p || NULL == M.p) {
		// realloc is shrink, should always success
		assert(0); abort();
	}
	minimized.resize_states(min_size);
	valvec<CharTarget<size_t> > children;
	children.reserve(sigma);
	for(state_id_t s2 = 0; s2 < min_size; ++s2) {
		state_id_t s1 = X.p[s2]; assert(M.p[s1] == s2);
		children.erase_all();
		for_each_move(s1, [&](state_id_t t, auchar_t c) {
			children.unchecked_push_back({c, M.p[t]});
			assert(M.p[t] < min_size);
		});
		minimized.add_all_move(s2, children);
		if (is_term(s1))
			minimized.set_term_bit(s2);
	}
}
template<class Au2>
void collapse(Hopcroft<state_id_t>& H, Au2& minimized) const {
	collapse3(H, minimized);
}


