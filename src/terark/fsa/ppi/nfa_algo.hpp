
class SinglePassPFS_Walker {
	febitvec		   color;
	valvec<state_id_t> stack;
	valvec<state_id_t> cache_epsilon;
	valvec<CharTarget<size_t> >  cache_non_epsilon;

public:
	SinglePassPFS_Walker(const MyType* nfa) {
		color.resize(nfa->total_states(), 0);
	}
	void clear_color() {
		color.fill(0);
	}
	void putRoot(state_id_t root) {
		assert(color.is0(root));
		color.set1(root);
		stack.push_back(root);
	}
	void discover_epsilon_children(const MyType* nfa, state_id_t curr) {
		nfa->get_epsilon_targets(curr, &cache_epsilon);
		// push to stack in reverse order
		for (size_t i = cache_epsilon.size(); i > 0; --i) {
			state_id_t  target = cache_epsilon[i-1];
			if (color.is0(target)) {
				color.set1(target);
				stack.push_back(target);
			}
		}
		cache_epsilon.erase_all();
	}
	void discover_non_epsilon_children(const MyType* nfa, state_id_t curr) {
		nfa->get_non_epsilon_targets(curr, &cache_non_epsilon);
		// push to stack in reverse order
		for (size_t i = cache_non_epsilon.size(); i > 0; --i) {
			state_id_t  target = cache_non_epsilon[i-1].target;
		#if !defined(NDEBUG)
			auchar_t ch = cache_non_epsilon[i-1].ch;
			assert(ch < nfa->get_sigma());
		#endif
			if (color.is0(target)) {
				color.set1(target);
				stack.push_back(target);
			}
		}
		cache_non_epsilon.erase_all();
	}
	state_id_t next() {
		assert(!stack.empty());
		state_id_t s = stack.back();
		stack.pop_back();
		return s;
	}
	bool is_finished() const { return stack.empty(); }

	// PFS search for epsilon closure
	void gen_epsilon_closure(const MyType* nfa, state_id_t source, valvec<state_id_t>* closure) {
		putRoot(source);
		size_t closure_oldsize = closure->size();
		while (!is_finished()) {
			state_id_t curr = next();
			closure->push_back(curr);
			discover_epsilon_children(nfa, curr);
		}
		// reset color of touched states backward:
		//   the last color[ps[*]] are more recently accessed
		//   and more likely in CPU cache
		bm_uint_t* pc = color.bldata();
		const size_t cbits = sizeof(pc[0]) * 8;
		const size_t ns = closure->size() - closure_oldsize;
		const state_id_t* ps = closure->data() + closure_oldsize - 1;
		for(size_t i = ns; i > 0; --i) pc[ps[i]/cbits] = 0;
	}
};

/// @param out dfa_states.back() is the guard
/// @param out power_set[dfa_states[i]..dfa_states[i+1]) is the nfa subset for dfa_state 'i'
/// @param out dfa
/// PowerIndex integer type: (uint32_t or uint64_t)
template<class PowerIndex, class Dfa>
size_t make_dfa(valvec<PowerIndex>* dfa_states, valvec<state_id_t>& power_set
			, Dfa* dfa, size_t max_power_set = size_t(-1), size_t timeout_us = 0) const {
//	BOOST_STATIC_ASSERT(Dfa::sigma >= MyType::sigma);
	terark::profiling  clock;
	uint64_t time_start = timeout_us ? clock.now() : 0;
	dfa->erase_all();
	power_set.erase_all();
	typename PowerSetMap<state_id_t, PowerIndex>::type power_set_map(power_set);
	// collect the targets of a dfa state:
	smallmap<NFA_SubSet<state_id_t> > dfa_next(this->get_sigma());
	SinglePassPFS_Walker walker(this);
	valvec<CharTarget<size_t> > children;
	walker.gen_epsilon_closure(this, initial_state, &power_set);
	std::sort(power_set.begin(), power_set.end());
	power_set.trim(std::unique(power_set.begin(), power_set.end()));
	assert(std::unique(power_set.begin(), power_set.end()) == power_set.end());
	power_set_map.enable_hash_cache();
	power_set_map.reserve(this->total_states() * 2);
	power_set_map.risk_size_inc(2);
	power_set_map.risk_size_dec(1);
	power_set_map.fast_begin()[0] = 0; // now size==1
	power_set_map.fast_begin()[1] = power_set.size();
	power_set_map.risk_insert_on_slot(0); // dfa initial_state
	//
	// power_set_map.end_i() may increase in iterations
	// when end_i() didn't changed, ds will catch up, then the loop ends.
	for (size_t ds = 0; ds < power_set_map.end_i(); ++ds) {
		if (timeout_us && (ds & 63) == 0) { // timeout check is expensive
			uint64_t time_curr = clock.now();
			if (uint64_t(clock.us(time_start, time_curr)) > timeout_us) {
				return 0; // timeout failed
			}
		}
		PowerIndex subset_beg = power_set_map.fast_begin()[ds+0];
		PowerIndex subset_end = power_set_map.fast_begin()[ds+1];
		dfa_next.resize0();
		for (PowerIndex j = subset_beg; j < subset_end; ++j) {
			state_id_t nfa_state_id = power_set[j];
			this->get_non_epsilon_targets(nfa_state_id, &children);
			for (size_t i = 0; i < children.size(); ++i) {
				CharTarget<size_t> t = children[i];
				walker.gen_epsilon_closure(this, t.target, &dfa_next.bykey(t.ch));
			}
			if (this->is_final(nfa_state_id)) dfa->set_term_bit(ds);
		}
		children.erase_all();
		for (size_t k = 0; k < dfa_next.size(); ++k) {
			NFA_SubSet<state_id_t>& nss = dfa_next.byidx(k);
			assert(!nss.empty());
			std::sort(nss.begin(), nss.end()); // sorted subset as Hash Key
			nss.trim(std::unique(nss.begin(), nss.end()));
			if (power_set.size() + nss.size() > max_power_set) {
				return 0; // failed
			}
			power_set.append(nss);        // add the new subset as Hash Key
			power_set_map.risk_size_inc(2);
			power_set_map.risk_size_dec(1);
			power_set_map.fast_end()[0] = power_set.size(); // new subset_end
			size_t New = power_set_map.end_i()-1; assert(New > 0);
			size_t Old = power_set_map.risk_insert_on_slot(New);
			if (New != Old) { // failed: the dfa_state: subset has existed
				power_set_map.risk_size_dec(1);
				power_set.resize(power_set_map.fast_end()[0]);
			}
			else if (dfa->total_states() < power_set_map.end_i()) {
				dfa->resize_states(power_set_map.capacity());
			}
		//	printf("New=%zd Old=%zd\n", New, Old);
			children.emplace_back(nss.ch, Old);
		}
		std::sort(children.begin(), children.end());
		dfa->add_all_move(ds, children);
	}
#ifdef TERARK_NFA_TO_DFA_PRINT_HASH_COLLISION
	printf("TERARK_NFA_TO_DFA_PRINT_HASH_COLLISION\n");
	valvec<int> collision;
	power_set_map.bucket_histogram(collision);
	for (int i = 0; i < (int)collision.size(); ++i) {
		printf("make_dfa: collision_len=%d cnt=%d\n", i, collision[i]);
	}
#endif
	dfa->resize_states(power_set_map.end_i());
	power_set.shrink_to_fit();
	if (dfa_states)
		dfa_states->assign(power_set_map.fast_begin(), power_set_map.fast_end()+1);
	return power_set.size();
}

template<class Dfa>
size_t make_dfa(Dfa* dfa, size_t max_power_set = size_t(-1), size_t timeout_us = 0) const {
	valvec<typename Dfa::state_id_t>* dfa_states = NULL;
	valvec<state_id_t> power_set;
	return make_dfa(dfa_states, power_set, dfa, max_power_set, timeout_us);
}

void write_one_state(long ls, FILE* fp) const {
	if (this->is_final(ls))
		fprintf(fp, "\tstate%ld[label=\"%ld\" shape=\"doublecircle\"];\n", ls, ls);
	else
		fprintf(fp, "\tstate%ld[label=\"%ld\"];\n", ls, ls);
}

void write_dot_file(FILE* fp) const {
	SinglePassPFS_Walker walker(this);
	walker.putRoot(initial_state);
	fprintf(fp, "digraph G {\n");
	while (!walker.is_finished()) {
		state_id_t curr = walker.next();
		write_one_state(curr, fp);
		walker.discover_epsilon_children(this, curr);
		walker.discover_non_epsilon_children(this, curr);
	}
	walker.clear_color();
	walker.putRoot(initial_state);
	valvec<CharTarget<size_t> > non_epsilon;
	valvec<state_id_t>     epsilon;
	while (!walker.is_finished()) {
		state_id_t curr = walker.next();
		assert(curr < this->total_states());
		this->get_epsilon_targets(curr, &epsilon);
		for (size_t j = 0; j < epsilon.size(); ++j) {
			long lcurr = curr;
			long ltarg = epsilon[j];
		//	const char* utf8_epsilon = "\\xCE\\xB5"; // dot may fail to show ε
			const char* utf8_epsilon = "&#949;";
			fprintf(fp, "\tstate%ld -> state%ld [label=\"%s\"];\n", lcurr, ltarg, utf8_epsilon);
		}
		this->get_non_epsilon_targets(curr, &non_epsilon);
		for (size_t j = 0; j < non_epsilon.size(); ++j) {
			long lcurr = curr;
			long ltarg = non_epsilon[j].target;
			int  label = non_epsilon[j].ch;
			if ('\\' == label || '"' == label)
				fprintf(fp, "\tstate%ld -> state%ld [label=\"\\%c\"];\n", lcurr, ltarg, label);
			else if (isgraph(label))
				fprintf(fp, "\tstate%ld -> state%ld [label=\"%c\"];\n", lcurr, ltarg, label);
			else
				fprintf(fp, "\tstate%ld -> state%ld [label=\"\\\\x%02X\"];\n", lcurr, ltarg, label);
		}
		walker.discover_epsilon_children(this, curr);
		walker.discover_non_epsilon_children(this, curr);
	}
	fprintf(fp, "}\n");
}

void write_dot_file(const char* fname) const {
	terark::Auto_fclose fp(fopen(fname, "w"));
	if (NULL == fp) {
		THROW_STD(runtime_error, "fopen(\"%s\", \"w\") = %s\n", fname, strerror(errno));
	}
	write_dot_file(fp);
}

