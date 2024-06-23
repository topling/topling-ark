void add_path(state_id_t source, state_id_t target, auchar_t ch) {
	this->add_move(source, target, ch);
}
template<class Char>
void add_path(state_id_t source, state_id_t target, const basic_fstring<Char> str) {
	assert(source < this->total_states());
	assert(target < this->total_states());
	if (0 == str.n) {
		this->add_epsilon(source, target);
	} else {
		state_id_t curr = source;
		for (size_t i = 0; i < str.size()-1; ++i) {
			auchar_t ch = str.uch(i);
			assert(ch < this->get_sigma());
			state_id_t next = this->new_state();
			this->add_move(curr, next, ch);
			curr = next;
		}
		this->add_move(curr, target, str.end()[-1]);
	}
}

void add_word(const fstring word) {
	add_word_aux(initial_state, word);
}
void add_word(state_id_t RootState, const fstring word) {
	add_word_aux(RootState, word);
}
void add_word16(const fstring16 word) {
	add_word_aux(initial_state, word);
}
void add_word16(state_id_t RootState, const fstring16 word) {
	add_word_aux(RootState, word);
}
template<class Char>
void add_word_aux(state_id_t RootState, const basic_fstring<Char> word) {
	state_id_t curr = RootState;
	for (size_t i = 0; i < word.size(); ++i) {
		state_id_t next = this->new_state();
		auchar_t ch = word.uch(i);
		assert(ch < this->get_sigma());
		this->add_move(curr, next, ch);
		curr = next;
	}
	this->set_final(curr);
}


