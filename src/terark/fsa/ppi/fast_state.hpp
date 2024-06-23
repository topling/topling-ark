#define FastState TERARK_PP_Identity(TERARK_PP_Arg0 ClassName_StateID_Sigma)
#define StateID   TERARK_PP_Identity(TERARK_PP_Arg1 ClassName_StateID_Sigma)
#define Sigma     TERARK_PP_Identity(TERARK_PP_Arg2 ClassName_StateID_Sigma)

class FastState {
public:
	enum MyEnumConst {
		MemPool_Align = sizeof(StateID),
		char_bits = Sigma > 256 ? 9 : 8,
		link_bits = sizeof(StateID) == 4 ? 32 : 61 - 2 * char_bits,
		sigma = Sigma,
	};
protected:
    // spos: saved position
    StateID spos  : link_bits; // spos*MemPool_Align is the offset to MemPool
    StateID minch : char_bits; // min char
    StateID maxch : char_bits; // max char, inclusive
    StateID term_bit : 1;
	StateID pzip_bit : 1;
	StateID free_bit : 1;
#if BOOST_JOIN(TERARK_PP_sizeof_,StateID) == 4
	StateID padding_bits : 64 - 32 - 2*char_bits - 3;
#endif
public:
    typedef StateID  state_id_t;
    typedef StateID  position_t;
    static const StateID nil_state = StateID((uint64_t(1)<<link_bits) - 1);
    static const StateID max_state = nil_state - 1;
	static const uint64_t max_pos1 = uint64_t(nil_state) * MemPool_Align;
	static const size_t max_pos = size_t(max_pos1 <= size_t(-1) ? max_pos1 : nil_state);

    FastState() {
        spos = nil_state;
        minch = maxch = 0;
		term_bit = 0;
		pzip_bit = 0;
		free_bit = 0;
#if BOOST_JOIN(TERARK_PP_sizeof_,StateID) == 4
		padding_bits = 0;
#endif
    }

	void setch(auchar_t ch) { minch = maxch = ch; }
	void setMinch(auchar_t ch) { minch = ch; }
	void setMaxch(auchar_t ch) { maxch = ch; }
	auchar_t getMaxch() const { return maxch; }
	auchar_t getMinch() const { return minch; }
	bool  range_covers(auchar_t ch) const { return ch >= minch && ch <= maxch; }
	int   rlen()      const { return maxch - minch + 1; }
	bool  has_children() const { return minch != maxch || spos != nil_state; }
	bool  more_than_one_child() const { return maxch != minch; }

    bool is_term() const { return term_bit; }
	bool is_pzip() const { return pzip_bit; }
	bool is_free() const { return free_bit; }
    void set_term_bit() { term_bit = 1; }
	void set_pzip_bit() { pzip_bit = 1; }
    void clear_term_bit() { term_bit = 0; }
	void set_free() { free_bit = 1; }
    void setpos(size_t tpos) {
        if (tpos >= max_pos) {
			THROW_STD(logic_error
				, "too large tpos=%zd, nil_state=%zd"
				, tpos, (size_t)nil_state);
		}
        this->spos = position_t(tpos / MemPool_Align);
    }
    size_t getpos() const {
        return size_t(spos) * MemPool_Align;
    }
    state_id_t get_target() const { return spos; }
    void set_target(state_id_t t) { spos = t;    }

	typedef state_id_t transition_t;
	static transition_t first_trans(state_id_t t) { return t; }
}; // end class FastState

BOOST_STATIC_ASSERT(sizeof(FastState) == 8);

#undef FastState
#undef StateID
#undef Sigma
#undef ClassName_StateID_Sigma
