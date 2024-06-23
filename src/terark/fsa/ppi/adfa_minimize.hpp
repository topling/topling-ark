protected:
struct HashEqByMap {
	const MyType*         au;
	const state_id_t* Min; // Map MyType.state_id to Minimized_DFA.state_id
	HashEqByMap(const MyType* au1, const state_id_t* M) : au(au1), Min(M) {}
	size_t hash(size_t x) const { return au->adfa_hash_hash(Min, x); }
	bool equal(size_t x,size_t y)const{return au->adfa_hash_equal(Min,x,y);}
};
template<class Au2>
struct ADFA_Minimize_Context : boost::noncopyable {
	typedef node_layout<state_id_t, state_id_t, FastCopy, ValueInline> Layout;
	typedef gold_hash_tab<state_id_t, state_id_t
		, HashEqByMap
		, terark_identity<state_id_t>
		, Layout
		> EquivalentRegister;
#ifdef TERARK_FSA_DEBUG
	valvec<state_id_t> L; // linked list, for checking algo correctness
#endif
	valvec<state_id_t> M; // map MyType state_id to Au2 state_id
	EquivalentRegister R; // many MyType.state_id may be equivlent

	ADFA_Minimize_Context(const MyType* au)
#ifdef TERARK_FSA_DEBUG
	  : L(au->total_states(), nil_state+0)
	  , M(au->total_states(), nil_state+0)
#else
	  : M(au->total_states(), valvec_no_init())
#endif
	  , R(HashEqByMap(au, M.data()))
	{
		R.set_load_factor(0.7);
		R.reserve(au->total_states());
	}

	// for topological_sort: on_finish
	void operator()(state_id_t state_id) {
		find_or_insert(state_id);
	}

	size_t find_or_insert(state_id_t state_id) {
		std::pair<size_t, bool> sb = R.insert_i(state_id);
	#ifdef TERARK_FSA_DEBUG
		if (!sb.second) {
			state_id_t& head = R.elem_at(sb.first);
			L[state_id] = head; // insert to head of linked list
			head = state_id; // update head
		}
		else assert(nil_state == L[R.elem_at(sb.first)]);
		assert(nil_state == M[state_id]);
	#endif
		M[state_id] = sb.first; // state id mapping
		return sb.first;
	}

	void collapse(const MyType* au, Au2& au2) {
		valvec<state_id_t> Heads;
		Heads.assign(R.begin(), R.end());
		R.clear();
	#ifdef TERARK_FSA_DEBUG
		for (size_t i = 0; i < M.size(); ++i) {
			assert(au->is_free(i) || nil_state != M[i]);
		}
	#endif
		size_t min_size = Heads.size();
		au2.resize_states(min_size);
		valvec<CharTarget<size_t> > children;
		children.reserve(sigma);
		for (size_t i = 0; i < min_size; ++i) {
			state_id_t curr = Heads[i];
			state_id_t parent2 = min_size - 1 - i;
			if (au->is_term(curr))
				au2.set_term_bit(parent2);
			children.erase_all();
			au->for_each_move(curr,
				[&](state_id_t child1, auchar_t ch) {
					assert(ch < Au2::sigma);
					state_id_t child2 = min_size - 1 - M[child1];
					children.unchecked_push_back({ch, child2});
				});
			au2.add_all_move(parent2, children);
		#ifdef TERARK_FSA_DEBUG
			while (nil_state != (curr = L[curr])) {
				assert(M[curr] == i);
				au->for_each_move(curr,
					[&](state_id_t child1, auchar_t ch) {
						state_id_t child2 = min_size - 1 - M[child1];
						state_id_t old = au2.state_move(parent2, ch);
						assert(old == child2);
					});
			}
		#endif
		}
		au2.shrink_to_fit();
	}
};

template<class Au2, class super>
struct ADFA_Minimize_ContextEx : super {
	const size_t* roots_beg;
	const size_t* roots_end;
	size_t* roots2_beg;

	ADFA_Minimize_ContextEx(const MyType* au
		, const valvec<size_t>& roots
		, valvec<size_t>* roots2
	) : super(au)
	{
		roots_beg = roots.begin();
		roots_end = roots.end();
		roots2->resize_no_init(roots.size());
		roots2->fill(Au2::nil_state+0);
		roots2_beg = roots2->begin();
	}

	// for topological_sort: on_finish
	void operator()(state_id_t state_id) {
		size_t min_state_id = this->find_or_insert(state_id);
		auto iter = std::lower_bound(roots_beg, roots_end, state_id);
		if (roots_end != iter && *iter == state_id) {
			size_t idx = iter - roots_beg;
			roots2_beg[idx] = min_state_id;
		}
	}
};

template<class Au2>
struct ADFA_Minimize_Context2 : boost::noncopyable {
	const MyType* m_au;
	size_t  m_num_states;
	size_t  m_num_bucket;
	size_t  m_min_size;
	AutoFree<state_id_t> m_base;

	ADFA_Minimize_Context2(const MyType* au) {
		m_au = au;
		m_min_size = 0;
		m_num_states = au->total_states();
		m_num_bucket = __hsm_stl_next_prime(m_num_states*3/2);
		size_t n_all = 2*m_num_states + m_num_bucket;
		m_base.alloc(n_all);
		std::fill_n(m_base.p, n_all, nil_state+0);
	}

	// for topological_sort: on_finish
	void operator()(state_id_t state_id) {
		this->find_or_insert(state_id);
	}

	void collapse(const MyType* au, Au2& au2) {
		typedef size_t cpu_word;
		enum { wbits = sizeof(cpu_word) * 8 };
		size_t min_size = m_min_size;
		size_t const bmwords = (min_size + wbits-1) / wbits;
		size_t const bmbytes = sizeof(cpu_word) * bmwords;
		// shrink memory by realloc:
		size_t fit_size = sizeof(state_id_t) * m_num_states + bmbytes;
		state_id_t* minmap = (state_id_t*)realloc(m_base.p, fit_size);
		cpu_word* bitmap = (cpu_word*)(minmap + m_num_states);
		// bitmap is re-using memory for hlinks
		memset(bitmap, 0, bmbytes);
		TERARK_VERIFY_F(nullptr != minmap, "realloc(%zd)", fit_size);
		m_base.p = minmap;
		au2.resize_states(min_size);
		valvec<CharTarget<size_t> > children;
		children.reserve(sigma);
		for(size_t parent1 = 0; parent1 < m_num_states; ++parent1) {
			state_id_t mapped = minmap[parent1];

			// should be a free state or untouchable state
			if (nil_state == mapped) continue;
			assert(mapped < min_size);

			// direct minmap is in reverse topological order
			// map of initial_state to initial_state should be in
			// topological order, reverse the direct minmap:
			size_t parent2 = min_size - 1 - mapped; // do reverse

			size_t idx = parent2/wbits;
			cpu_word bit = cpu_word(1) << parent2%wbits;
			if (bitmap[idx] & bit)
				continue;
			bitmap[idx] |= bit;

			if (au->is_term(parent1))
				au2.set_term_bit(parent2);
			children.erase_all();
			au->for_each_move(parent1,
			[&](size_t child1, auchar_t ch) {
				assert(ch < Au2::sigma);
				assert(nil_state != minmap[child1]);
				state_id_t child2 = min_size - 1 - minmap[child1];
				children.unchecked_push_back({ch, child2});
			});
			au2.add_all_move(parent2, children);
		}
		au2.shrink_to_fit();
	}

	size_t find_or_insert(size_t xkey) {
		// maunally crafted specialized hash table insertion:
		assert(m_min_size < m_num_states);
		state_id_t* minmap = m_base.p;
		state_id_t* hlinks = minmap + 1*m_num_states;
		state_id_t* bucket = minmap + 2*m_num_states;
		size_t hash = m_au->adfa_hash_hash(minmap, xkey);
		size_t hpos = hash % m_num_bucket;
		size_t ykey = bucket[hpos];
		assert(ykey != xkey);
		while (ykey != nil_state) {
			if (m_au->adfa_hash_equal(minmap, xkey, ykey)) {
				return minmap[xkey] = minmap[ykey];
			}
			ykey = hlinks[ykey];
		}
		size_t min_size = m_min_size++;
		hlinks[xkey] = bucket[hpos];
		minmap[xkey] = min_size;
		bucket[hpos] = xkey;
		return min_size;
	}
};

/// 'this' must be Acyclic DFA
template<class MinContext, class Au2>
void adfa_minimize_aux(Au2& au2) const {
	assert(num_zpath_states() == 0);
	au2.clear();
	MinContext ctx(this);
	this->template topological_sort<MinContext&>(ctx);
	ctx.collapse(this, au2);
}

template<class MinContext, class Au2>
void
adfa_minimize_aux(const valvec<size_t>& roots, Au2& au2,
			  valvec<size_t>* roots2
) const {
#ifdef TERARK_FSA_DEBUG
	// must be sorted
	for (size_t i = 1; i < roots.size(); ++i) {
		assert(roots[i-1] < roots[i]);
	}
	assert(num_zpath_states() == 0);
#endif
	au2.clear();
	valvec<state_id_t> stack(roots.size(), valvec_no_init());
	for(size_t i = 0; i < stack.size(); ++i) {
		assert(roots[i] < this->total_states());
		stack[i] = roots[i];
	}
	MinContext ctx(this, roots, roots2);
	this->template topological_sort<MinContext&>(stack, ctx);
	ctx.collapse(this, au2);
	for (auto& root : *roots2)
		root = au2.total_states() - 1 - root;
}

public:
template<class Au2>
void adfa_minimize(Au2& au2) const {
	typedef ADFA_Minimize_Context<Au2> ctx_t;
	adfa_minimize_aux<ctx_t, Au2>(au2);
}
template<class Au2>
void adfa_minimize2(Au2& au2) const {
	typedef ADFA_Minimize_Context2<Au2> ctx_t;
	adfa_minimize_aux<ctx_t, Au2>(au2);
}

template<class Au2>
void
adfa_minimize(const valvec<size_t>& roots, Au2& au2,
			  valvec<size_t>* roots2
) const {
	typedef ADFA_Minimize_ContextEx<Au2, ADFA_Minimize_Context<Au2> >
			ctx_t;
	adfa_minimize_aux<ctx_t, Au2>(roots, au2, roots2);
}
template<class Au2>
void
adfa_minimize2(const valvec<size_t>& roots, Au2& au2,
			  valvec<size_t>* roots2
) const {
	typedef ADFA_Minimize_ContextEx<Au2, ADFA_Minimize_Context2<Au2> >
			ctx_t;
	adfa_minimize_aux<ctx_t, Au2>(roots, au2, roots2);
}


