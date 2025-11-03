public:
void reserve_states(size_t cap) {
	if (cap >= this->max_state) {
		cap = this->max_state;
	}
	this->states.reserve(cap);
}
bool add_word(fstring word) {
	return add_word_aux(initial_state, word).second;
}
bool add_word(size_t RootState, fstring word) {
	return add_word_aux(RootState, word).second;
}
bool add_word16(fstring16 word) {
	return add_word_aux(initial_state, word).second;
}
bool add_word16(size_t RootState, fstring16 word) {
	return add_word_aux(RootState, word).second;
}

/// @returns pair(tail-state, is-inserted-as-new)
///  is-inserted-as-new is equivalent as `Not Existed`
template<class Char>
std::pair<size_t, bool>
add_word_aux(size_t RootState, basic_fstring<Char> word) {
	assert(0 == num_zpath_states());
	assert(RootState < total_states());
	if (this->m_zpath_states) {
		THROW_STD(invalid_argument, "DFA must not be path_zip'ed");
	}
	size_t curr = RootState;
	size_t next;
	size_t j = 0;
	while (j < word.size()) {
		next = state_move(curr, word.uch(j));
		if (nil_state == next)
			goto AddNewStates;
		assert(next < total_states());
		curr = next;
		++j;
	}
	// word could be the prefix of some existed word word
	if (is_term(curr)) {
		return std::make_pair(curr, false);
	} else {
		set_term_bit(curr);
		return std::make_pair(curr, true);
	}
AddNewStates:
	do {
		next = new_state();
		add_move_checked(curr, next, word.uch(j));
		curr = next;
		++j;
	} while (j < word.size());
	// now curr == next, and it is a Terminal State
	set_term_bit(curr);
	return std::make_pair(curr, true);
}

void append_delim_value(auchar_t delim, fstring value) {
	valvec<state_id_t> finals;
	get_final_states(&finals);
	for (size_t i = 0; i < finals.size(); ++i) {
		state_id_t root = finals[i];
		if (state_move(root, delim) != nil_state) {
			char msg[128];
			sprintf(msg, "dfa_mutate.append_delim_value: has delim=0x%02X(%c)", delim, delim);
			throw std::logic_error(msg);
		}
	}
	state_id_t next = new_state();
	for (size_t i = 0; i < finals.size(); ++i) {
		state_id_t root = finals[i];
		add_move(root, next, delim);
		this->clear_term_bit(root);
	}
	add_word(next, value);
}

void fast_append(auchar_t delim, fstring word) {
	fast_append_aux(delim, word);
}
void fast_append16(auchar_t delim, fstring16 word) {
	fast_append_aux(delim, word);
}
template<class Char>
void fast_append_aux(auchar_t delim, basic_fstring<Char> word) {
	assert(this->num_free_states() == 0);
	size_t word_root = this->new_state();
	this->add_word_aux(word_root, word);
	size_t num_old_finals = 0;
	for (size_t i = 0; i < word_root; ++i) {
		if (this->is_term(i)) {
			this->clear_term_bit(i);
			this->add_move_checked(i, word_root, delim);
			num_old_finals++;
		}
	}
	if (0 == num_old_finals) {
		THROW_STD(invalid_argument, "this has no any final states");
	}
}

///@param out path
///@return
///  - nil_state   : the str from 'from' to 'to' is newly created
///  - same as 'to' : the str from 'from' to 'to' has existed
///  - others       : the str from 'from' to 'XX' has existed, XX is not 'to'
///                   in this case, the Automata is unchanged
///@note
///  - This function don't change the Automata if str from 'from' to 'to'
///    has existed. The caller should check the return value if needed.
template<class Char>
state_id_t make_path( state_id_t from, state_id_t to
					, const basic_fstring<Char> str
					, valvec<state_id_t>* path) {
	assert(num_zpath_states() == 0);
	assert(nil_state != from);
	assert(nil_state != to);
	assert(NULL != path);
	assert(NULL != str.p);
#ifdef NDEBUG
	if (0 == str.n) return false;
#else
	assert(str.n > 0); // don't allow empty str
#endif
	auto& p = *path;
	p.resize(str.n+1);
	p[0] = from;
	for (ptrdiff_t j = 0; j < str.n-1; ++j) {
		if (nil_state == (p[j+1] = state_move(p[j], str.uch(j)))) {
			do add_move(p[j], p[j+1] = new_state(), str.uch(j));
			while (++j < str.n-1);
			break;
		}
		assert(p[j+1] < total_states());
	}
	state_id_t old_to = add_move(p[str.n-1], to, str.uch(str.n-1));
	p[str.n] = (nil_state == old_to) ? to : old_to;
	return old_to;
}

// in most cases this function is fast enough
// in extreme cases, assuming you need to copy many small DFA
// preallocate a WorkingCacheOfCopyFrom object could
// reduce malloc/realloc/free and a little faster
template<class Au2>
state_id_t
copy_from(const Au2& au2, size_t SrcRoot = initial_state) {
	WorkingCacheOfCopyFrom<MyType, Au2> ctx(au2.total_states());
	return ctx.copy_from(this, au2, SrcRoot);
}
template<class Au2>
state_id_t
copy_from(state_id_t DstRoot, const Au2& au2, size_t SrcRoot = initial_state) {
	WorkingCacheOfCopyFrom<MyType, Au2> ctx(au2.total_states());
	return ctx.copy_from(this, DstRoot, au2, SrcRoot);
}

state_id_t
fast_copy(size_t SrcBeg) {
	assert(SrcBeg < total_states());
	return fast_copy(*this, SrcBeg, total_states() - SrcBeg);
}

state_id_t
fast_copy(size_t SrcBeg, size_t SrcNum) {
	assert(SrcBeg < total_states());
	assert(SrcBeg + SrcNum <= total_states());
	return fast_copy(*this, SrcBeg, SrcNum);
}

template<class SrcDFA>
state_id_t // use typename SrcDFA::state_id_t SrcBeg for SFINAE
fast_copy(const SrcDFA& src, typename SrcDFA::state_id_t SrcBeg = 0) {
	assert(SrcBeg < src.total_states());
	return fast_copy(src, SrcBeg, src.total_states() - SrcBeg);
}

template<class SrcDFA>
state_id_t
fast_copy(const SrcDFA& src, size_t SrcBeg, size_t SrcNum) {
	assert(SrcBeg < src.total_states());
	assert(SrcBeg + SrcNum <= src.total_states());
	if (this->total_states() + SrcNum >= max_state) {
		THROW_STD(out_of_range, "Will be exceed max_state");
	}
	size_t root = total_states();
	this->resize_states(root + SrcNum);
	return fast_copy(root, src, SrcBeg, SrcNum);
}

template<class SrcDFA>
state_id_t // use typename SrcDFA::state_id_t SrcBeg for SFINAE
fast_copy(size_t DstBeg, const SrcDFA& src, typename SrcDFA::state_id_t SrcBeg = 0) {
	assert(SrcBeg < src.total_states());
	return fast_copy(DstBeg, src, SrcBeg, src.total_states() - SrcBeg);
}

template<class SrcDFA>
state_id_t
fast_copy(size_t DstBeg, const SrcDFA& src, size_t SrcBeg, size_t SrcNum) {
	assert(SrcBeg < src.total_states());
	assert(SrcBeg + SrcNum <= src.total_states());
	if (SrcBeg + SrcNum > src.total_states()) {
		THROW_STD(invalid_argument, "SrcBeg + SrcNum > src.total_states()");
	}
	assert(DstBeg + SrcNum <= total_states());
	terark::AutoFree<CharTarget<size_t> > children(src.get_sigma());
	for (size_t i = 0; i < SrcNum; ++i) {
		if (src.is_free(SrcBeg + i)) {
			this->del_state(DstBeg + i);
		}
		else {
			size_t n = src.get_all_move(SrcBeg + i, children);
			if (n) {
				for(size_t k = 0; k < n; ++k) {
					size_t target = children[k].target;
					assert(target <  SrcBeg + SrcNum);
					assert(target >= SrcBeg);
					assert(!src.is_free(target));
					children[k].target = DstBeg + (target - SrcBeg);
				}
				this->add_all_move(DstBeg + i, children, n);
			}
			if (src.is_term(SrcBeg + i))
				this->set_term_bit(DstBeg + i);
		}
	}
	return DstBeg;
}

////////////////////////////////////////////////////////////
// Virtuals

const BaseDFA* get_BaseDFA()
const override final {	return this; }

// non-virtual
void add_move_checked(size_t source, size_t target, auchar_t ch) {
	assert(ch < sigma);
	size_t old = this->add_move_imp(source, target, ch, false);
	assert(nil_state == old);
	if (nil_state != old) {
		throw std::logic_error(BOOST_CURRENT_FUNCTION);
	}
}
size_t add_move(size_t source, size_t target, auchar_t ch) {
	assert(ch < sigma);
	return this->add_move_imp(source, target, ch, false);
}
// overwrite existed
/// @return nil_state : if not existed
///         otherwise  : the old target which was overwritten
size_t set_move(size_t source, size_t target, auchar_t ch) {
	assert(ch < sigma);
	return this->add_move_imp(source, target, ch, true);
}

