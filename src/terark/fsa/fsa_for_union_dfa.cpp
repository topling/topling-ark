#include "fsa_for_union_dfa.hpp"
#include <terark/util/hugepage.hpp>
#include <terark/util/throw.hpp>
#include <terark/util/fstrvec.hpp>

#if defined(__clang__) // || defined(__GNUC__) || defined(__GNUG__)
	// clang warn typid usage in this file
	#pragma clang diagnostic ignored "-Wpotentially-evaluated-expression"
#endif

namespace terark {

FSA_ForUnionDFA::FSA_ForUnionDFA() {
	m_total_states = 0;
	m_max_sigma = 0;
	m_free_states = 0;
	m_is_owner = false;
}

FSA_ForUnionDFA::~FSA_ForUnionDFA() {
	clear();
}

void FSA_ForUnionDFA::clear() {
	if (m_is_owner) {
		for (size_t i = 0; i < m_dfas.size(); ++i)
			delete const_cast<BaseDFA*>(m_dfas[i]);
	}
	m_dfas.clear();
	m_index.clear();
	m_total_states = 0;
}

void FSA_ForUnionDFA::add_dfa(const BaseDFA* dfa) {
	m_dfas.push_back(dfa);
}

void FSA_ForUnionDFA::get_all_root(valvec<size_t>* roots) const {
	roots->resize_no_init(m_index.size());
	std::copy(m_index.begin(), m_index.end(), roots->begin());
}

void FSA_ForUnionDFA::compile() {
	m_index.resize_no_init(m_dfas.size());
	size_t root = 1; // 0 is reserved for initial_state
	m_max_sigma = 256;
	m_free_states = 0;
	for (size_t i = 0; i < m_index.size(); ++i) {
		auto dfa = m_dfas[i];
		m_index[i] = root;
		root += dfa->v_total_states();
		if (root >= max_state) {
			THROW_STD(out_of_range, "root=%zd max_state=%zd"
				, root, (size_t)max_state);
		}
		m_max_sigma = std::max(m_max_sigma, dfa->get_sigma());
	//	m_free_states += dfa->v_num_free_states();
	}
	m_total_states = root;
	m_stack_dests.reserve(2 * m_max_sigma);
	m_stack_moves.reserve(2 * m_max_sigma);
}

bool FSA_ForUnionDFA::is_final(size_t s) const {
	assert(s < m_total_states);
	if (initial_state == s) {
		return false;
	}
	auto i = std::upper_bound(m_index.begin(), m_index.end(), s) - 1;
	assert(i >= m_index.begin());
	auto root = *i;
	auto dfa = m_dfas[i - m_index.begin()];
	assert(s - root < dfa->v_total_states());
	return dfa->v_is_term(s - root);
}
void FSA_ForUnionDFA::get_epsilon_targets(size_t s, valvec<state_id_t>* children) const {
	assert(s < m_total_states);
	children->erase_all();
	if (initial_state == s) {
		for (auto root : m_index)
			children->push_back(root);
	}
}
void FSA_ForUnionDFA::get_non_epsilon_targets(size_t s, valvec<CharTarget<size_t> >* children)
const {
	assert(s < m_total_states);
	children->erase_all();
	if (initial_state == s) {
		return;
	}
	auto i = std::upper_bound(m_index.begin(), m_index.end(), s) - 1;
	assert(i >= m_index.begin());
	size_t root = *i;
	auto dfa = m_dfas[i - m_index.begin()];
	assert(s - root < dfa->v_total_states());
	dfa->get_all_move(s - root, children);
	auto beg = children->data();
	size_t len = children->size();
	for (size_t i = 0; i < len; ++i) {
		beg[i].target += root;
	}
}

// DFA operations: the FSA is DFA except initial_state
// the DFA has multiple roots, and can be minimized by Hopcroft/adfa_minimize
//
size_t FSA_ForUnionDFA::get_all_dest(size_t s, size_t* dests) const {
	assert(s != initial_state);
	assert(s < m_total_states);
	auto i = std::upper_bound(m_index.begin(), m_index.end(), s) - 1;
	assert(i >= m_index.begin());
	size_t root = *i;
	auto dfa = m_dfas[i - m_index.begin()];
	assert(s - root < dfa->v_total_states());
	size_t cnt = dfa->get_all_dest(s - root, dests);
	for (size_t j = 0; j < cnt; ++j) dests[j] += root;
	return cnt;
}
size_t FSA_ForUnionDFA::get_all_move(size_t s, CharTarget<size_t>* moves) const {
	assert(s != initial_state);
	assert(s < m_total_states);
	auto i = std::upper_bound(m_index.begin(), m_index.end(), s) - 1;
	assert(i >= m_index.begin());
	size_t root = *i;
	auto dfa = m_dfas[i - m_index.begin()];
	assert(s - root < dfa->v_total_states());
	size_t cnt = dfa->get_all_move(s - root, moves);
	for (size_t j = 0; j < cnt; ++j) moves[j].target += root;
	return cnt;
}

size_t FSA_ForUnionDFA::risk_get_all_dest(size_t s, size_t* pcnt) const {
	assert(s != initial_state);
	assert(s < m_total_states);
	auto i = std::upper_bound(m_index.begin(), m_index.end(), s) - 1;
	assert(i >= m_index.begin());
	size_t root = *i;
	auto dfa = m_dfas[i - m_index.begin()];
	assert(s - root < dfa->v_total_states());
	size_t oldsize = m_stack_dests.size();
	m_stack_dests.resize_no_init(oldsize + m_max_sigma);
	auto dests = m_stack_dests.begin() + oldsize;
	size_t cnt = dfa->get_all_dest(s - root, dests);
	for (size_t i = 0; i < cnt; ++i) dests[i] += root;
	m_stack_dests.risk_set_size(oldsize + cnt);
	*pcnt = cnt;
	return oldsize;
}

size_t FSA_ForUnionDFA::risk_get_all_move(size_t s, size_t* pcnt) const {
	assert(s != initial_state);
	assert(s < m_total_states);
	auto i = std::upper_bound(m_index.begin(), m_index.end(), s) - 1;
	assert(i >= m_index.begin());
	size_t root = *i;
	auto dfa = m_dfas[i - m_index.begin()];
	assert(s - root < dfa->v_total_states());
	size_t oldsize = m_stack_moves.size();
	m_stack_moves.resize_no_init(oldsize + m_max_sigma);
	auto moves = m_stack_moves.begin() + oldsize;
	size_t cnt = dfa->get_all_move(s - root, moves);
	for (size_t i = 0; i < cnt; ++i) moves[i].target += root;
	m_stack_moves.risk_set_size(oldsize + cnt);
	*pcnt = cnt;
	return oldsize;
}

bool FSA_ForUnionDFA::is_dag() const {
	for (auto dfa : m_dfas)
		if (!dfa->is_dag())
			return false;
	return true;
}

bool FSA_ForUnionDFA::is_free(size_t s) const {
#if 0 // !defined(NDEBUG)
	auto i = std::upper_bound(m_index.begin(), m_index.end(), s) - 1;
	assert(i >= m_index.begin());
	size_t root = *i;
	auto dfa = m_dfas[i - m_index.begin()];
	assert(s - root < dfa->v_total_states());
	return dfa->v_is_free(s - root);
#else
	return false;
#endif
}

size_t FSA_ForUnionDFA::adfa_hash_hash(const state_id_t* Min, size_t s) const {
	assert(s != initial_state);
	assert(s < m_total_states);
	auto i = std::upper_bound(m_index.begin(), m_index.end(), s) - 1;
	assert(i >= m_index.begin());
	auto root = *i;
	auto dfa = m_dfas[i - m_index.begin()];
	assert(s - root < dfa->v_total_states());
	size_t h = dfa->v_is_term(s - root) ? 1 : 0;
	size_t oldsize = m_stack_moves.size();
	m_stack_moves.resize_no_init(oldsize + m_max_sigma);
	CharTarget<size_t>* moves = m_stack_moves.data() + oldsize;
	size_t cnt = dfa->get_all_move(s - root, moves);
	for (size_t i = oldsize; i < oldsize + cnt; ++i) {
		h = FaboHashCombine(h, Min[moves[i].target + root]);
		h = FaboHashCombine(h, moves[i].ch);
	}
	h = FaboHashCombine(h, cnt);
	m_stack_moves.risk_set_size(oldsize);
	return h;
}

bool
FSA_ForUnionDFA::adfa_hash_equal(const state_id_t* Min, size_t x, size_t y)
const {
	assert(x != initial_state);
	assert(y != initial_state);
	assert(x < m_total_states);
	assert(y < m_total_states);
	const BaseDFA *dfa1, *dfa2;
	size_t root1, root2;
  {
	auto i = std::upper_bound(m_index.begin(), m_index.end(), x) - 1;
	assert(i >= m_index.begin());
	dfa1 = m_dfas[i - m_index.begin()];
	root1 = *i;
	assert(x - root1 < dfa1->v_total_states());
	i = std::upper_bound(m_index.begin(), m_index.end(), y) - 1;
	assert(i >= m_index.begin());
	dfa2 = m_dfas[i - m_index.begin()];
	root2 = *i;
	assert(y - root2 < dfa2->v_total_states());
  }
	if (dfa1->v_is_term(x - root1) != dfa2->v_is_term(y - root2))
		return false;
	auto oldsize = m_stack_moves.size();
	m_stack_moves.resize_no_init(oldsize + 2*m_max_sigma);
	auto xmoves = m_stack_moves.data() + oldsize;
	auto xcnt = dfa1->get_all_move(x - root1, xmoves);
	auto ymoves = xmoves + xcnt;
	auto ycnt = dfa2->get_all_move(y - root2, ymoves);
	bool ret = false;
	if (xcnt == ycnt) {
		for (size_t i = 0; i < xcnt; ++i) {
			if (xmoves[i].ch != ymoves[i].ch)
				goto Done;
			size_t tx = xmoves[i].target + root1;
			size_t ty = ymoves[i].target + root2;
			if (tx != ty && Min[tx] != Min[ty])
				goto Done;
		}
		ret = true;
	}
Done:
	m_stack_moves.risk_set_size(oldsize);
	return ret;
}

///////////////////////////////////////////////////////////////
#if TERARK_WORD_BITS >= 64
struct TermSubMap {
	TermSubMap() : subdfa_id(UINT32_MAX), substate_id(UINT32_MAX)
	{}
	TermSubMap(uint32_t _subdfa_id, uint32_t _substate_id)
		: subdfa_id(_subdfa_id)
		, substate_id(_substate_id)
	{}
	uint32_t subdfa_id = UINT32_MAX;
	uint32_t substate_id = UINT32_MAX;
};

struct LazyUnion_DA_State {
	uint32_t m_child0; // aka base, the child state on transition: '\0'
	uint32_t m_parent; // aka check
	uint32_t m_map_sub_state;
	uint32_t m_subdfa_id : 32 - MAX_ZPATH_LEN_BITS;
	uint32_t m_zstr_idx  : MAX_ZPATH_LEN_BITS;
	uint32_t m_is_term   :  1;

	typedef uint32_t state_id_t;
	typedef uint32_t position_t;
	static const uint32_t max_state = 0xFFFFFFFE;
	static const uint32_t nil_state = 0xFFFFFFFF;

	LazyUnion_DA_State() {
		m_child0 = nil_state;
		m_parent = nil_state;
		m_map_sub_state = nil_state;
		m_is_term = false;
		m_zstr_idx = MAX_ZPATH_LEN_MASK;
		m_subdfa_id = (uint32_t(1) << 18) - 1;
	}

	void set_term_bit() { m_is_term = true; }
	void set_free_bit() { m_map_sub_state = nil_state; }
	void set_child0(state_id_t x) { assert(x < max_state); m_child0 = x; }
	void set_parent(state_id_t x) { assert(x < max_state); m_parent = x; }

	bool is_term() const { return 0 != m_is_term; }
	bool is_free() const { return nil_state == m_parent; }
	uint32_t child0() const { /*assert(!is_free());*/ return m_child0; }
	uint32_t parent() const { /*assert(!is_free());*/ return m_parent; }

	typedef state_id_t transition_t;
	static transition_t first_trans(state_id_t t) { return t; }
};

struct LazyUnion_DA_DAWG_State : public LazyUnion_DA_State {
	size_t n_presum; // dawg prefix sum num on edge
	size_t n_suffix;
};
class FastDynaDFA;
class LazyUnion_DA_Trie {
	size_t m_gnode_states;
	size_t m_dfa_mem_size;
	size_t m_dfa_states;
	double m_build_time;
public:
	valvec<LazyUnion_DA_State> m_states;
	valvec<TermSubMap>  m_term_submap;
	enum { sigma = 512 };
	static const uint32_t nil_state = UINT32_MAX;
	static const uint32_t max_state = UINT32_MAX-1;

	size_t get_full_state(const LazyUnion_DA_State* da, size_t t) const;
	size_t state_move(size_t state, auchar_t ch) const;
	/// this is much slower than Automata::for_each_move
	template<class OP>
	void for_each_move(size_t curr, OP op) const;
	template<class OP>
	void for_each_dest(size_t curr, OP op) const;
	template<class OP>
	void for_each_dest_rev(size_t curr, OP op) const;

	void build(const FastDynaDFA* trie, const char* walkMethod);
	void print_cache_stat(FILE* fp) const;
};

#if 1
class DynaFastState {
public:
	enum MyEnumConst {
		MemPool_Align = 16,
		sigma = 512,
	};
	size_t  pos_or_id; // pos_or_id*MemPool_Align is the offset to MemPool
	size_t  n_suffix : 45;
	size_t  minch    :  9; // min char
	size_t  maxch    :  9; // max char, inclusive
	size_t  term_bit :  1;

	typedef size_t  state_id_t;
	typedef size_t  position_t;
	static const size_t nil_state = size_t(-1);
	static const size_t max_state = nil_state-1;
	static const size_t max_pos   = max_state;

	DynaFastState() {
		pos_or_id = nil_state;
		minch = maxch = 0;
		term_bit = 0;
	}

	void setch(auchar_t ch) { minch = maxch = ch; }
	void setMinch(auchar_t ch) { minch = ch; }
	void setMaxch(auchar_t ch) { maxch = ch; }
	auchar_t getMaxch() const { return maxch; }
	auchar_t getMinch() const { return minch; }
	bool  range_covers(auchar_t ch) const { return ch >= minch && ch <= maxch; }
	int   rlen()      const { return maxch - minch + 1; }
	bool  has_children() const { return minch != maxch || pos_or_id != nil_state; }
	bool  more_than_one_child() const { return maxch != minch; }

	bool is_term() const { return term_bit; }
	bool is_pzip() const { return false; }
	bool is_free() const { return false; }
	void set_term_bit() { term_bit = 1; }
//	void set_pzip_bit();
	void clear_term_bit() { term_bit = 0; }
	void set_free() { assert(0); }
	void setpos(size_t tpos) {
		if (tpos >= max_pos) {
			THROW_STD(logic_error
				, "too large tpos=%zd, nil_state=%zd"
				, tpos, (size_t)nil_state);
		}
		this->pos_or_id = position_t(tpos / MemPool_Align);
	}
	size_t getpos() const { return size_t(pos_or_id) * MemPool_Align; }
	size_t get_target() const { return pos_or_id; }
	void set_target(state_id_t t) { pos_or_id = t; }

	struct transition_t {
		size_t target;
		size_t num;
		transition_t(size_t t) : target(t), num(-1) {}
		transition_t(size_t t, size_t num) : target(t), num(num) {}
		operator size_t() const { return target; }
	};
	static transition_t
		first_trans(state_id_t t) { return transition_t(t, 0); }
}; // end class DynaFastState
BOOST_STATIC_ASSERT(sizeof(DynaFastState) == 16);
#else
typedef PackedState<size_t, 64, 4, 512> DynaFastState;
BOOST_STATIC_ASSERT(sizeof(State6B_512) == 10);
#endif

class LazyUnion_DA_Trie;
class LazyUnionDAWG;
class FastDynaDFA : public Automata<DynaFastState> {
	friend class LazyUnionDAWG;
	friend class LazyUnion_DA_Trie;
public:
	valvec<TermSubMap> m_term_submap;
	void clear() {
		m_term_submap.clear();
		Automata<DynaFastState>::clear();
	}
};

template<class StdFunc>
struct GetFunc {
	template<class AnyFunc>
	static const StdFunc
	get(AnyFunc& f, boost::mpl::true_/*IsRef*/) { return ref(f); }

	template<class AnyFunc>
	static const StdFunc
	get(const AnyFunc& f, boost::mpl::true_/*IsRef*/) { return cref(f); }

	template<class AnyFunc>
	static const StdFunc
	get(const AnyFunc& f, boost::mpl::false_/*IsRef*/) { return f; }

	static const StdFunc&
	get(const StdFunc& f, boost::mpl::true_/*IsRef*/) { return f; }
};

static bool g_LazyUnionDFA_use_fast_iter = true;
TERARK_DLL_EXPORT void LazyUnionDFA_setFastIter(bool val) {
	g_LazyUnionDFA_use_fast_iter = val;
}
TERARK_DLL_EXPORT bool LazyUnionDFA_getFastIter() {
	return g_LazyUnionDFA_use_fast_iter;
}

class LazyUnionDFA : public BaseLazyUnionDFA {
protected:
	typedef LazyUnionDFA MyType;
public:
	enum { sigma = 512 };
	struct ZstrCache;
	// MAX_STATE_BITS = subdfa_num_bits + state_num_bits + MAX_ZPATH_LEN_BITS
	enum { state_num_bits = 32 };
	enum { subdfa_num_bits = MAX_STATE_BITS - state_num_bits - MAX_ZPATH_LEN_BITS };
	static size_t get_zstr_idx(size_t s) { return  s & MAX_ZPATH_LEN_MASK; }
	static size_t get_state_id(size_t s) { return (s >> MAX_ZPATH_LEN_BITS) & ((size_t(1) << state_num_bits) - 1); }
	static size_t get_subdfa_id(size_t s) { return s >> (MAX_STATE_BITS-subdfa_num_bits); }
	static size_t make_id(size_t dfa_id, size_t state_id, size_t zidx) {
		assert(  dfa_id < size_t(1) << subdfa_num_bits);
		assert(state_id < size_t(1) <<  state_num_bits);
		assert(zidx <= MAX_ZPATH_LEN);
		return dfa_id << (MAX_STATE_BITS - subdfa_num_bits) | state_id << MAX_ZPATH_LEN_BITS | zidx;
	}
	FastDynaDFA m_dyndfa;
	LazyUnion_DA_Trie m_da_trie; // faster than m_dyndfa;
	valvec<const MatchingDFA*> m_subdfa; // m_subdfa[0] is reserved
	size_t m_real_sigma; // different from BaseDFA::m_dyn_sigma
	size_t m_gnode_states;
	fstrvec m_dfa_str_bounds; // size = m_subdfa.size(), [i] <-->subdfa[i]
	bool   m_subdfa_is_same_class;
	bool   m_is_owner;
public:
	typedef size_t state_id_t;
	typedef size_t transition_t;
	static const size_t nil_state = size_t(-1);
	static const size_t max_state = size_t(-1) - 1;
	static const size_t max_base_state = (size_t(1)<<(state_num_bits + MAX_ZPATH_LEN_BITS))-1;

	LazyUnionDFA();
	~LazyUnionDFA();

	size_t total_states() const;
//	size_t num_used_states() const { return total_states(); }
//	size_t num_free_states() const { return 1; }
	size_t allsub_states() const;
	size_t mem_size() const override final;
	size_t v_gnode_states() const override final;

	void set_own(bool v) { m_is_owner = v; }
	void add_dfa(const MatchingDFA*);

	void compile() { compile(size_t(-1)); }
	virtual void compile(size_t PowerLimit); // do dfa_union

	size_t find_sub_dfa(fstring key) const final;

	bool has_freelist() const override final;
	bool is_free(size_t state_id) const;
	bool is_pzip(size_t state_id) const;
	bool is_term(size_t state_id) const;
	fstring get_zpath_data(size_t, MatchContext*) const;

	size_t v_num_children(size_t s) const override;
	bool v_has_children(size_t s) const override final;

	size_t state_move(size_t parent, auchar_t ch) const;

	void for_each_move(size_t parent, const OnMove&) const;
	void for_each_dest(size_t parent, const OnDest&) const;
	void for_each_dest_rev(size_t parent, const OnDest&) const;

	void for_each_move(size_t parent, const OnMove* op) const {
		 for_each_move(parent, *op); }
	void for_each_dest(size_t parent, const OnDest* op) const {
		 for_each_dest(parent, *op); }
	void for_each_dest_rev(size_t parent, const OnDest* op) const {
		 for_each_dest_rev(parent, *op); }

	template<class OP>
	void for_each_move(size_t parent, OP op) const {
		 for_each_move(parent, OnMove(ref(op))); }
	template<class OP>
	void for_each_dest(size_t parent, OP op) const {
		 for_each_dest(parent, OnDest(ref(op))); }
	template<class OP>
	void for_each_dest_rev(size_t parent, OP op) const {
		 for_each_dest_rev(parent, OnDest(ref(op))); }

protected:
	size_t for_each_word(size_t root, size_t zidx, const OnNthWord& f) const override;
	size_t for_each_suffix(MatchContext&, fstring, const OnNthWord&) const override;
	size_t for_each_value(MatchContext&, const OnMatchKey&) const override;
	size_t prepend_for_each_value(size_t,size_t,fstring,size_t,size_t,const OnMatchKey&) const override;
	size_t prepend_for_each_word (size_t,fstring,size_t,size_t,const OnNthWord &) const override;
	size_t prepend_for_each_word_ex(size_t,fstring,size_t,size_t,const OnNthWordEx&) const override;

	size_t match_key(MatchContext&, auchar_t, fstring, const OnMatchKey&) const override;
	size_t match_key(MatchContext&, auchar_t, fstring, const OnMatchKey&, const ByteTR&) const override;
	size_t match_key(MatchContext&, auchar_t, ByteInputRange&, const OnMatchKey&) const override;
	size_t match_key(MatchContext&, auchar_t, ByteInputRange&, const OnMatchKey&, const ByteTR&) const override;

	size_t match_key_l(MatchContext&, auchar_t, fstring, const OnMatchKey&) const override;
	size_t match_key_l(MatchContext&, auchar_t, fstring, const OnMatchKey&, const ByteTR&) const override;
	size_t match_key_l(MatchContext&, auchar_t, ByteInputRange&, const OnMatchKey&) const override;
	size_t match_key_l(MatchContext&, auchar_t, ByteInputRange&, const OnMatchKey&, const ByteTR&) const override;

	bool step_key(MatchContext&, auchar_t, fstring) const override;
	bool step_key(MatchContext&, auchar_t, fstring, const ByteTR&) const override;
	bool step_key_l(MatchContext&, auchar_t, fstring) const override;
	bool step_key_l(MatchContext&, auchar_t, fstring, const ByteTR&) const override;

	template<class TR>
	size_t
	tpl_match_key(MatchContext&, auchar_t, fstring, const OnMatchKey&, TR) const;

	template<class TR>
	size_t
	tpl_match_key_l(MatchContext&, auchar_t, fstring, const OnMatchKey&, TR) const;

	template<class TR>
	size_t stream_match_key
	(MatchContext&, auchar_t, ByteInputRange&, const OnMatchKey&, TR) const;

	template<class TR>
	size_t stream_match_key_l
	(MatchContext&, auchar_t, ByteInputRange&, const OnMatchKey&, TR) const;

	template<class TR>
	bool
	tpl_step_key(MatchContext& ctx, auchar_t delim, fstring str, TR tr) const;

	template<class TR>
	bool
	tpl_step_key_l(MatchContext& ctx, auchar_t delim, fstring str, TR tr) const;

	class PrefixDFA; friend class PrefixDFA;
	class ForEachWordOnPrefix; friend class ForEachWordOnPrefix;
	class ForEachValueOnPrefix; friend class ForEachValueOnPrefix;

#include "ppi/match_path.hpp"
#include "ppi/dfa_const_virtuals.hpp"
#include "ppi/state_move_fast.hpp"
#define TERARK_FSA_has_override_adfa_make_iter
#include "ppi/adfa_iter.hpp"
#undef TERARK_FSA_has_override_adfa_make_iter
#include "ppi/match_pinyin.hpp"

template<class CharT>
class FastLexIteratorT : public ADFA_LexIteratorT<CharT> {
	typedef ADFA_LexIteratorT<CharT> super;
	typedef basic_fstring<CharT> fstr;
	typedef typename fstr::uc_t  uc_t;
	using super::m_dfa;
	using super::m_word;
	using super::m_curr;
	super* m_sub_iter;
	size_t m_sub_idx;

public:
	FastLexIteratorT(const LazyUnionDFA* dfa) : super(valvec_no_init()) {
		m_sub_iter = dfa->m_subdfa[1]->adfa_make_iter();
		m_sub_idx = 1;
		m_dfa = dfa;
	}
	~FastLexIteratorT() {
		this->m_word.risk_release_ownership();
		//delete m_sub_iter;
		m_sub_iter->dispose();
	}

	void reset(const BaseDFA* dfa, size_t root) override {
		assert(nullptr != dfa);
		auto pDFA = dynamic_cast<const LazyUnionDFA*>(dfa);
		assert(nullptr != pDFA);
		if (nullptr == pDFA) {
			THROW_STD(invalid_argument,
				"dfa instance is not the expected class");
		}
		size_t nstates = pDFA->total_states();
		assert(root < nstates);
		if (root >= nstates) {
			THROW_STD(invalid_argument,
				"root = %zd, states = %zd", root, nstates);
		}
		m_sub_iter->dispose();
		m_sub_iter = nullptr;
		m_sub_iter = pDFA->m_subdfa[1]->adfa_make_iter();
		m_dfa = dfa;
		m_curr = size_t(-1);
		m_word.risk_set_size(0);
	}

	void set_result(super* sub_iter, size_t dfa_id) {
		// derived class can not access another base class object's
		// protected members, so treat sub_iter as FastLexIteratorT
		// and only access its base (protected) members
		auto s = static_cast<FastLexIteratorT*>(sub_iter);
		m_curr = make_id(dfa_id, s->m_curr, 0);
		m_word.risk_set_data(s->m_word.data(), s->m_word.size());
	}

	bool incr() override {
		auto sub_iter = m_sub_iter;
		if (terark_likely(sub_iter->incr())) {
			set_result(sub_iter, m_sub_idx);
			return true;
		}
		MyType const* dfa = static_cast<const MyType*>(m_dfa);
		if (m_sub_idx < dfa->m_subdfa.size()-1) {
			m_sub_idx++;
			m_sub_iter->reset(dfa->m_subdfa[m_sub_idx], initial_state);
			bool ret = m_sub_iter->seek_begin();
			set_result(sub_iter, m_sub_idx);
			return ret;
		}
		set_result(sub_iter, m_sub_idx);
		return false;
	}

	bool decr() override {
		auto sub_iter = m_sub_iter;
		if (terark_likely(sub_iter->decr())) {
			set_result(sub_iter, m_sub_idx);
			return true;
		}
		MyType const* dfa = static_cast<const MyType*>(m_dfa);
		if (m_sub_idx > 1) {
			m_sub_idx--;
			m_sub_iter->reset(dfa->m_subdfa[m_sub_idx], initial_state);
			bool ret = m_sub_iter->seek_end();
			set_result(sub_iter, m_sub_idx);
			return ret;
		}
		set_result(sub_iter, m_sub_idx);
		return false;
	}

	bool seek_begin() override {
		MyType const* dfa = static_cast<const MyType*>(m_dfa);
		auto sub_iter = m_sub_iter;
		m_sub_idx = 1;
		m_sub_iter->reset(dfa->m_subdfa[1], initial_state);
		bool ret = m_sub_iter->seek_begin();
		set_result(sub_iter, m_sub_idx);
		return ret;
	}

	bool seek_end() override {
		MyType const* dfa = static_cast<const MyType*>(m_dfa);
		auto sub_iter = m_sub_iter;
		m_sub_idx = dfa->m_subdfa.size() - 1;;
		m_sub_iter->reset(dfa->m_subdfa[m_sub_idx], initial_state);
		bool ret = m_sub_iter->seek_end();
		set_result(sub_iter, m_sub_idx);
		return ret;
	}

	bool seek_lower_bound(fstr key) override {
		MyType const* dfa = static_cast<const MyType*>(m_dfa);
		const char* base = dfa->m_dfa_str_bounds.strpool.data();
		auto sub_iter = m_sub_iter;
		size_t subidx = lower_bound_ex_n(
			const_cast<uint32_t*>(dfa->m_dfa_str_bounds.offsets.data()),
			1, dfa->m_dfa_str_bounds.offsets.size()-1,
			key,
			[base](uint32_t& offset) {
				size_t offset0 = offset;
				size_t offset1 = (&offset)[1];
				size_t length = offset1 - offset0;
				return fstring(base + offset0, length);
			});
		if (subidx < dfa->m_dfa_str_bounds.size()) {
			sub_iter->reset(dfa->m_subdfa[subidx], initial_state);
			m_sub_idx = subidx;
			bool ret = sub_iter->seek_lower_bound(key);
			set_result(sub_iter, m_sub_idx);
			return ret;
		}
		return false;
	}

	size_t seek_max_prefix(fstr key) override {
		THROW_STD(invalid_argument, "Not implemeted");
	}
};

virtual ADFA_LexIterator* adfa_make_iter(size_t root = initial_state)
const override {
	if (g_LazyUnionDFA_use_fast_iter && m_subdfa_is_same_class)
		return new FastLexIteratorT<char>(this);
	else
		return new MyLexIteratorT<char>(this);
}

virtual ADFA_LexIterator16* adfa_make_iter16(size_t root = initial_state)
const override {
//	if (g_LazyUnionDFA_use_fast_iter && m_subdfa_is_same_class)
//		return new FastLexIteratorT<uint16_t>(this);
//	else
		return new MyLexIteratorT<uint16_t>(this);
}

#define TERARK_FSA_has_match_prefix_impl
#include "ppi/match_prefix.hpp"

#if defined(TERARK_FSA_has_match_prefix_impl)
template<class OnMatch, class TR>
size_t
tpl_match_prefix(MatchContext& ctx, fstring str, OnMatch on_prefix, TR tr)
const {
	if (terark_unlikely(ctx.root || ctx.pos || ctx.zidx)) {
		THROW_STD(invalid_argument, "ctx.{root,pos,zidx} must be all 0");
	}
	if (auto da = m_da_trie.m_states.data()) {
		size_t state = 0;
		for(size_t i = 0; ; ++i) {
			assert(state < m_da_trie.m_states.size());
			size_t map_sub_state = da[state].m_map_sub_state;
			if (terark_unlikely(UINT32_MAX != map_sub_state)) {
				size_t state_zidx = da[state].m_zstr_idx;
				size_t dfa_id = da[state].m_subdfa_id;
				assert(dfa_id > 0);
				assert(dfa_id < m_subdfa.size());
				const MatchingDFA* dfa = m_subdfa[dfa_id];
				assert(map_sub_state < dfa->v_total_states());
				if (state_zidx) {
					ctx.zidx = state_zidx - 1;
				}
				ctx.root = map_sub_state;
				ctx.pos = i;
				return dfa->match_prefix(ctx, str
					, GetFunc<OnPrefix>::get(on_prefix, boost::is_reference<OnMatch>())
					, GetFunc<ByteTR>::get(tr, boost::is_reference<TR>()));
			}
			assert(i <= str.size());
			if (da[state].is_term())
				on_prefix(i);
			if (str.size() == i)
				return i;
			byte_t ch = (byte_t)tr((byte_t)str.p[i]);
			size_t child = da[state].child0() + ch;
			assert(child < m_da_trie.m_states.size());
			if (da[child].parent() == state)
				state = child;
			else
				return i;
		}
	}
	else {
		size_t full_state = 0;
		for(size_t i = 0; ; ++i) {
			size_t sub_state_id = get_state_id(full_state);
			if (full_state >= max_base_state) {
				size_t state_zidx = get_zstr_idx(full_state);
				size_t dfa_id = get_subdfa_id(full_state);
				assert(dfa_id > 0);
				assert(dfa_id < m_subdfa.size());
				auto dfa = m_subdfa[dfa_id];
				if (state_zidx) {
					ctx.zidx = state_zidx - 1;
				}
				ctx.root = sub_state_id;
				ctx.pos = i;
				return dfa->match_prefix(ctx, str
					, GetFunc<OnPrefix>::get(on_prefix, boost::is_reference<OnMatch>())
					, GetFunc<ByteTR>::get(tr, boost::is_reference<TR>()));
			}
			assert(i <= str.size());
			assert(sub_state_id < m_dyndfa.total_states());
			if (m_dyndfa.is_term(sub_state_id))
				on_prefix(i);
			if (str.size() == i)
				return i;
			byte_t ch = (byte_t)tr((byte_t)str.p[i]);
			size_t full_child = m_dyndfa.state_move(sub_state_id, ch);
			if (nil_state == full_child)
				return i;
			assert(full_child < total_states());
			full_state = full_child;
		}
	}
	assert(0);
	return size_t(-1); // should not goes here...
}

template<class Range, class OnMatch, class TR>
size_t
stream_match_prefix(MatchContext& ctx, Range r, OnMatch on_prefix, TR tr)
const {
	if (terark_unlikely(ctx.root || ctx.pos || ctx.zidx)) {
		THROW_STD(invalid_argument, "ctx.{root,pos,zidx} must be all 0");
	}
	if (auto da = m_da_trie.m_states.data()) {
		size_t state = 0;
		for(size_t i = 0; ; ++i) {
			assert(state < m_da_trie.m_states.size());
			size_t map_sub_state = da[state].m_map_sub_state;
			if (terark_unlikely(UINT32_MAX != map_sub_state)) {
				size_t state_zidx = da[state].m_zstr_idx;
				size_t dfa_id = da[state].m_subdfa_id;
				assert(dfa_id > 0);
				assert(dfa_id < m_subdfa.size());
				const MatchingDFA* dfa = m_subdfa[dfa_id];
				assert(map_sub_state < dfa->v_total_states());
				if (state_zidx) {
					ctx.zidx = state_zidx - 1;
				}
				ctx.root = map_sub_state;
				ctx.pos = i;
				return dfa->match_prefix(ctx, r
					, GetFunc<OnPrefix>::get(on_prefix, boost::is_reference<OnMatch>())
					, GetFunc<ByteTR>::get(tr, boost::is_reference<TR>()));
			}
			if (da[state].is_term())
				on_prefix(i);
			if (r.empty())
				return i;
			byte_t ch = (byte_t)tr((byte_t)*r);
			size_t child = da[state].child0() + ch;
			assert(child < m_da_trie.m_states.size());
			if (da[child].parent() == state)
				state = child;
			else
				return i;
		}
	}
	else {
		size_t full_state = 0;
		for(size_t i = 0; ; ++i) {
			size_t sub_state_id = get_state_id(full_state);
			if (full_state >= max_base_state) {
				size_t state_zidx = get_zstr_idx(full_state);
				size_t dfa_id = get_subdfa_id(full_state);
				assert(dfa_id > 0);
				assert(dfa_id < m_subdfa.size());
				auto dfa = m_subdfa[dfa_id];
				if (state_zidx) {
					ctx.zidx = state_zidx - 1;
				}
				ctx.root = sub_state_id;
				ctx.pos = i;
				return dfa->match_prefix(ctx, r
					, GetFunc<OnPrefix>::get(on_prefix, boost::is_reference<OnMatch>())
					, GetFunc<ByteTR>::get(tr, boost::is_reference<TR>()));
			}
			assert(sub_state_id < m_dyndfa.total_states());
			if (m_dyndfa.is_term(sub_state_id))
				on_prefix(i);
			if (r.empty())
				return i;
			byte_t ch = (byte_t)tr((byte_t)*r);
			size_t full_child = m_dyndfa.state_move(sub_state_id, ch);
			if (nil_state == full_child)
				return i;
			assert(full_child < total_states());
			full_state = full_child;
		}
	}
	assert(0);
	return size_t(-1); // should not goes here...
}
#endif
#undef TERARK_FSA_has_match_prefix_impl
};

inline size_t
LazyUnion_DA_Trie::get_full_state(const LazyUnion_DA_State* da, size_t child)
const {
	auto zidx = da[child].m_zstr_idx;
	auto map_sub_state = da[child].m_map_sub_state;
	if (UINT32_MAX == map_sub_state) {
		return LazyUnionDFA::make_id(0, child, zidx);
	} else {
		size_t subdfa_id = da[child].m_subdfa_id;
		return LazyUnionDFA::make_id(subdfa_id, map_sub_state, zidx);
	}
}

size_t LazyUnion_DA_Trie::state_move(size_t state, auchar_t ch) const {
	assert(state < m_states.size());
	auto da = m_states.data();
	size_t child = da[state].child0() + ch;
	if (da[child].parent() == state)
		return get_full_state(da, child);
	else
		return LazyUnionDFA::nil_state;
}

/// this is much slower than Automata::for_each_move
template<class OP>
void LazyUnion_DA_Trie::for_each_move(size_t curr, OP op) const {
	assert(curr < m_states.size());
	assert(!m_states[curr].is_free());
	auto da = m_states.data();
	size_t child0 = da[curr].child0();
	for (int ch = 0; ch < sigma; ++ch) {
		size_t child = child0 + ch;
		if (da[child].parent() == curr) {
			auto t = get_full_state(da, child);
			op(t, (auchar_t)ch);
		}
	}
}
template<class OP>
void LazyUnion_DA_Trie::for_each_dest(size_t curr, OP op) const {
	assert(!m_states[curr].is_free());
	auto da = m_states.data();
	size_t child0 = da[curr].child0();
	for (int ch = 0; ch < sigma; ++ch) {
		size_t child = child0 + ch;
		if (da[child].parent() == curr) {
			auto t = get_full_state(da, child);
			op(t);
		}
	}
}
template<class OP>
void LazyUnion_DA_Trie::for_each_dest_rev(size_t curr, OP op) const {
	assert(!m_states[curr].is_free());
	auto da = m_states.data();
	size_t child0 = m_states[curr].child0();
	for (int ch = sigma-1; ch >= 0; --ch) {
		size_t child = child0 + ch;
		if (da[child].parent() == curr) {
			auto t = get_full_state(da, child);
			op(t);
		}
	}
}

static const char* sanitizeWalkMethod(const char* walkMethod) {
	if (walkMethod) {
		return walkMethod;
	}
	if (auto env = getenv("UnionPart_walkMethod")) {
		if (strcasecmp(env, "BFS") == 0
			|| strcasecmp(env, "CFS") == 0
			|| strcasecmp(env, "DFS") == 0
		) {
			walkMethod = env;
		}
		else {
			walkMethod = "DFS";
			fprintf(stderr
				, "WARN: env UnionPart_walkMethod=%s is invalid, use default: \"%s\"\n"
				, env, walkMethod);
		}
	}
	return walkMethod;
}
struct CharTargetNum {
	size_t  ch;
	size_t  target;
	size_t  num;
};
void
LazyUnion_DA_Trie::build(const FastDynaDFA* trie, const char* walkMethod) {
	walkMethod = sanitizeWalkMethod(walkMethod);
	profiling pf;
	long long t1 = pf.now();
	valvec<uint32_t> t2d(trie->total_states()    , UINT32_MAX);
//	valvec<uint32_t> d2t(trie->total_states()*3/2, UINT32_MAX);
	valvec<LazyUnion_DA_State> d_states(trie->total_states()*3/2);
	AutoFree<CharTargetNum> labels(512);
	const size_t extra = 0;
	size_t curr_slot = 1;
	size_t real_size = 1;
	DFS_TreeWalker<size_t> walker;
	walker.resize(trie->total_states());
	walker.putRoot(initial_state);
	t2d[initial_state] = initial_state;
	d_states[initial_state].set_parent(initial_state);
	if (trie->is_term(initial_state))
		d_states[initial_state].set_term_bit();
	m_gnode_states = 1;
	while (!walker.is_finished()) {
		const size_t t_parent = walker.next();
		assert(t_parent < trie->total_states());
		size_t n_moves = 0;
		trie->for_each_move(t_parent,
			[&](DynaFastState::transition_t t, auchar_t ch) {
				labels[n_moves].ch     = ch;
				labels[n_moves].target = t.target;
				labels[n_moves].num    = t.num;
				n_moves++;
			});
		const size_t prev_slot = curr_slot;
	Retry:
		for(size_t i = 0; i < n_moves; ++i) {
			size_t   ch    = labels.p[i].ch;
			size_t d_child = curr_slot + ch;
			if (d_child >= d_states.size()) {
				d_states.resize(d_child*3/2);
			//	d2t.resize(d_child*3/2, UINT32_MAX);
			}
			else if (!d_states[d_child].is_free()) {
				do {
					if (++d_child == d_states.size()) {
						curr_slot =  d_states.size();
						goto Retry;
					}
				} while (!d_states[d_child].is_free());
				curr_slot = d_child - ch;
				if (1 == n_moves) break;
				else goto Retry;
			}
		}
		size_t d_parent = t2d[t_parent];
		assert(d_parent < d_states.size());
		d_states[d_parent].set_child0(curr_slot);
		size_t n_real = 0;
		for(size_t i = 0; i < n_moves; ++i) {
			size_t t_child = labels[i].target;
			auto     ch    = labels[i].ch;
			size_t d_child = curr_slot + ch;
			size_t subdfa_id = LazyUnionDFA::get_subdfa_id(t_child);
			size_t state_id = LazyUnionDFA::get_state_id(t_child);
			size_t zstr_idx = LazyUnionDFA::get_zstr_idx(t_child);
			d_states[d_child].set_parent(d_parent);
			d_states[d_child].m_subdfa_id = subdfa_id;
		//	d_states[d_child].num_dawg = 0;
			d_states[d_child].m_zstr_idx = zstr_idx;
			if (subdfa_id) {
				d_states[d_child].m_map_sub_state = state_id;
			}
			else {
				if (trie->is_term(state_id)) {
					d_states[d_child].set_term_bit();
					assert(state_id < trie->m_term_submap.size());
					m_term_submap.ensure_set(d_child,
							trie->m_term_submap[state_id]);
				}
				t2d[state_id] = d_child;
				labels.p[n_real].ch = ch;
				labels.p[n_real].target = state_id;
				labels.p[n_real].num = labels[i].num;
				n_real++;
			}
			m_gnode_states++;
			if (d_child >= real_size)
				real_size = d_child + 1;
		}
		walker.putChildrenT(labels.p, n_real);
		size_t huristic_factor = 16;
		curr_slot = prev_slot + (curr_slot - prev_slot)/huristic_factor;
	}
//	d2t.resize(real_size + extra + sigma, UINT32_MAX);
//	d2t.shrink_to_fit();
	d_states.resize(real_size + extra + sigma);
	d_states.shrink_to_fit();
	for (size_t i = 0; i < real_size; i++) {
		if (d_states[i].child0() == nil_state)
			d_states[i].set_child0(0);
	}
	d_states[initial_state].set_parent(initial_state);
	m_term_submap.shrink_to_fit();
	long long t2 = pf.now();
	this->m_dfa_mem_size = trie->mem_size();
	this->m_dfa_states = trie->total_states();
	this->m_build_time = pf.sf(t1, t2);
	if (d_states.used_mem_size() >= hugepage_size) {
		use_hugepage_advise(&d_states);
	}
	m_states.swap(d_states);
	if (getEnvBool("UnionPart_showStat")) {
		print_cache_stat(stderr);
	}
}

void LazyUnion_DA_Trie::print_cache_stat(FILE* fp) const {
	size_t memsize = m_states.used_mem_size();
	size_t nstates = m_states.size();
	fprintf(fp, "UnionPart build  time: %9.6f sec | %8.3f MB/sec\n", m_build_time, memsize/1e6/m_build_time);
	fprintf(fp, "UnionPart total bytes: %9zd     | all = %11zd | ratio = %7.5f\n", memsize, m_dfa_mem_size, 1.0*memsize/m_dfa_mem_size);
	fprintf(fp, "UnionPart total nodes: %9zd     | all = %11zd | ratio = %7.5f\n", m_states.size(), m_dfa_states, 1.0*nstates / m_dfa_states);
	fprintf(fp, "UnionPart  used nodes: %9zd\n", m_gnode_states);
	fprintf(fp, "UnionPart  fill ratio: %9.6f\n", m_gnode_states*1.0/nstates);
	fprintf(fp, "UnionPart nodes bytes: %9zd\n", nstates);
	fprintf(fp, "UnionPart node1 bytes: %9zd\n", sizeof(LazyUnion_DA_State));
}

class LazyUnionDAWG : public LazyUnionDFA, public SuffixCountableDAWG {
protected:
	bool m_has_suffix_cnt;
	valvec<size_t> m_word_num_vec;

public:
	void compile(size_t PowerLimit) override;

protected:
	void compile_loop(size_t parent);

	size_t index_nlt(MatchContext&, fstring word) const;
	size_t index_nlt_da(MatchContext&, fstring word) const;
	size_t index_dawg(MatchContext&, fstring word) const;
protected:
	size_t index(MatchContext&, fstring word) const override;
    void lower_bound(MatchContext&, fstring word, size_t* index, size_t* dict_rank) const override;
	void nth_word(MatchContext&, size_t nth, std::string* word) const override;
	DawgIndexIter dawg_lower_bound(MatchContext&, fstring word) const override;
	size_t v_state_to_word_id(size_t state) const override;
	size_t state_to_dict_rank(size_t state) const override;

	size_t match_dawg(MatchContext&, size_t, fstring, const OnMatchDAWG&) const override;
	size_t match_dawg(MatchContext&, size_t, fstring, const OnMatchDAWG&, const ByteTR&) const override;
	size_t match_dawg(MatchContext&, size_t, fstring, const OnMatchDAWG&, const byte_t*) const override;

	bool match_dawg_l(MatchContext&, fstring, size_t* len, size_t* nth) const override;
	bool match_dawg_l(MatchContext&, fstring, size_t* len, size_t* nth, const ByteTR&) const override;
	bool match_dawg_l(MatchContext&, fstring, size_t* len, size_t* nth, const byte_t*) const override;

	size_t suffix_cnt(size_t root) const override;
	size_t suffix_cnt(MatchContext&, fstring) const override;
	size_t suffix_cnt(MatchContext&, fstring, const ByteTR&) const override;
	size_t suffix_cnt(MatchContext&, fstring, const byte_t*) const override;

	void path_suffix_cnt(MatchContext&, fstring, valvec<size_t>*) const override;
	void path_suffix_cnt(MatchContext&, fstring, valvec<size_t>*, const ByteTR&) const override;
	void path_suffix_cnt(MatchContext&, fstring, valvec<size_t>*, const byte_t*) const override;

	template<class TR>
	size_t tpl_suffix_cnt(MatchContext& ctx, fstring str, TR tr) const;

	template<class TR>
	void tpl_path_suffix_cnt(MatchContext&, fstring, valvec<size_t>*, TR) const;

	template<class TR>
	size_t tpl_match_dawg(MatchContext&, size_t, fstring, const OnMatchDAWG&, TR) const;

	const BaseDAWG* get_dawg() const override final;
	const SuffixCountableDAWG* get_SuffixCountableDAWG() const override;

	static inline size_t
	suffix_cnt_sub(const SuffixCountableDAWG* dawg, MatchContext& ctx,
				fstring str, IdentityTR) {
		return dawg->suffix_cnt(ctx, str);
	}
	static inline size_t
	suffix_cnt_sub(const SuffixCountableDAWG* dawg, MatchContext& ctx,
				fstring str, const ByteTR& tr) {
		return dawg->suffix_cnt(ctx, str, tr);
	}
	static inline size_t
	suffix_cnt_sub(const SuffixCountableDAWG* dawg,MatchContext& ctx,
				fstring str, TableTranslator tr) {
		return dawg->suffix_cnt(ctx, str, tr.tr_tab);
	}
	static inline void
	path_suffix_cnt_sub(const SuffixCountableDAWG* dawg, MatchContext& ctx,
				fstring str, valvec<size_t>* cnt, IdentityTR) {
		dawg->path_suffix_cnt(ctx, str, cnt);
	}
	static inline void
	path_suffix_cnt_sub(const SuffixCountableDAWG* dawg, MatchContext& ctx,
				fstring str, valvec<size_t>* cnt, const ByteTR& tr) {
		dawg->path_suffix_cnt(ctx, str, cnt, tr);
	}
	static inline void
	path_suffix_cnt_sub(const SuffixCountableDAWG* dawg, MatchContext& ctx,
				fstring str, valvec<size_t>* cnt, TableTranslator tr) {
		dawg->path_suffix_cnt(ctx, str, cnt, tr.tr_tab);
	}

	static inline size_t
	match_dawg_sub(const BaseDAWG* dawg, MatchContext& ctx, size_t base_nth,
				fstring str, const OnMatchDAWG& f, IdentityTR) {
		return dawg->match_dawg(ctx, base_nth, str, f);
	}
	static inline size_t
	match_dawg_sub(const BaseDAWG* dawg, MatchContext& ctx, size_t base_nth,
				fstring str, const OnMatchDAWG& f, const ByteTR& tr) {
		return dawg->match_dawg(ctx, base_nth, str, f, tr);
	}
	static inline size_t
	match_dawg_sub(const BaseDAWG* dawg, MatchContext& ctx, size_t base_nth,
				fstring str, const OnMatchDAWG& f, TableTranslator tr) {
		return dawg->match_dawg(ctx, base_nth, str, f, tr.tr_tab);
	}
};

struct LazyUnionDFA::ZstrCache {
	gold_hash_map<size_t, size_t> m_zstr_cache;
	valvec<byte_t> m_zstr_pool;
	MatchContext   m_zbuf_ctx;

	fstring get(const BaseDFA* dfa, size_t state_id) {
		fstring zstr;
		size_t sub_state_id = get_state_id(state_id);
		if (dfa->zp_nest_level()) { // lookup cache
			size_t key = state_id & ~MAX_ZPATH_LEN_MASK;
			size_t offset = m_zstr_pool.size(); // offset to m_zstr_pool
			std::pair<size_t, bool> ib = m_zstr_cache.insert_i(key, offset);
			if (ib.second) {
				zstr = dfa->v_get_zpath_data(sub_state_id, &m_zbuf_ctx);
				m_zstr_pool.append(zstr);
				zstr.p = (char*)m_zstr_pool.data() + offset;
			}
			else {
				offset = m_zstr_cache.val(ib.first);
				size_t nextOffset = m_zstr_cache.end_i() - 1 == ib.first
					? m_zstr_pool.size()
					: m_zstr_cache.val(ib.first + 1)
					;
				zstr.p = (char*)m_zstr_pool.data() + offset;
				zstr.n = nextOffset - offset;
			}
		}
		else {
			zstr = dfa->v_get_zpath_data(sub_state_id, NULL);
		}
		return zstr;
	}
};

LazyUnionDFA::LazyUnionDFA() {
	m_is_owner = false;
	m_subdfa_is_same_class = true;
	m_subdfa.reserve(64);
	m_subdfa.push_back(NULL);
	m_dyn_sigma = sigma;
	m_real_sigma = 0;
	m_gnode_states = 0;
}

LazyUnionDFA::~LazyUnionDFA() {
	if (m_is_owner) {
		// don't delete m_subdfa[0]
		for (size_t i = m_subdfa.size()-1; i > 0; --i)
			delete const_cast<MatchingDFA*>(m_subdfa[i]);
	}
}

size_t LazyUnionDFA::total_states() const {
/*
	fprintf(stderr,
	 "%s: this function retuans a much larger value than real total_states\n"
	 , BOOST_CURRENT_FUNCTION);
*/
	return make_id(m_subdfa.size(), 0, 0);
}

size_t LazyUnionDFA::allsub_states() const {
	size_t sum = 0;
	for (size_t i = 1; i < m_subdfa.size(); ++i)
		sum += m_subdfa[i]->v_total_states();
	return sum;
}
size_t LazyUnionDFA::mem_size() const {
	size_t sum = m_dyndfa.mem_size();
	for (size_t i = 1; i < m_subdfa.size(); ++i)
		sum += m_subdfa[i]->mem_size();
	return sum;
}

void LazyUnionDFA::add_dfa(const MatchingDFA* dfa) {
	m_subdfa.push_back(dfa);
}
void LazyUnionDFA::compile(size_t PowerLimit) {
	assert(m_subdfa.size() > 2);
	m_real_sigma = 0;
	m_gnode_states = 0;
	bool b_is_dag = 1;
	for (size_t i = 1; i < m_subdfa.size(); ++i) {
		m_real_sigma = std::max(m_real_sigma, m_subdfa[i]->get_sigma());
		m_gnode_states += m_subdfa[i]->v_gnode_states();
		b_is_dag &= m_subdfa[i]->is_dag();
		if (!b_is_dag) THROW_STD(logic_error, "%zd'th DFA is not a dag", i);
	}
	this->set_is_dag(b_is_dag);
//	fprintf(stderr, "%s: b_is_dag=%d\n", BOOST_CURRENT_FUNCTION, b_is_dag);
	const size_t pmap_initial_size = 4096;
	SubSetHashMap<size_t>  pmap(pmap_initial_size);
	valvec<size_t> roots(m_subdfa.size()-1, valvec_no_init());
	for(size_t i = 0; i < roots.size(); ++i) {
		const BaseDFA* dfa = m_subdfa[i+1];
		size_t zidx = dfa->v_is_pzip(initial_state) ? 1 : 0;
		roots[i] = make_id(i+1, initial_state, zidx);
	}
	pmap.find_or_add_subset(roots);
	smallmap<NFA_SubSet<size_t> > next_sets(m_real_sigma);
	valvec<CharTarget<size_t> > children;
	ZstrCache zcache;
#define BFS_tail pmap.size() /* pmap is also a bfs queue */
	for(size_t bfshead = 0; bfshead < BFS_tail; ++bfshead) {
		size_t sub_beg = pmap.node[bfshead+0].index;
		size_t sub_end = pmap.node[bfshead+1].index;
		bool b_is_final = false;
		next_sets.resize0();
		for(size_t i = sub_beg; i < sub_end; ++i) {
			size_t parent = pmap.data[i];
			size_t sub_state_id = get_state_id(parent);
			if (parent < max_base_state) {
				assert(0); // should not goes here
				m_dyndfa.for_each_move(sub_state_id,
					[&](size_t child, auchar_t ch) {
						next_sets.bykey(ch).push_back(child);
					});
				b_is_final = b_is_final || m_dyndfa.is_term(sub_state_id);
			}
			else {
				size_t dfa_id = get_subdfa_id(parent);
				const BaseDFA* dfa = m_subdfa[dfa_id];
				if (dfa->v_is_pzip(sub_state_id)) {
					auto   zstr = zcache.get(dfa, parent);
					size_t zidx = get_zstr_idx(parent);
					assert(zidx > 0);
					if (zidx <= zstr.size()) // 'parent + 1' is the unique child
						next_sets.bykey(zstr[zidx-1]).push_back(parent + 1);
					else
						goto DoGetAllMove;
				} else { DoGetAllMove:
					dfa->get_all_move(sub_state_id, &children);
					for(size_t j = 0; j < children.size(); ++j) {
						CharTarget<size_t> ct = children[j];
						size_t zidx = dfa->v_is_pzip(ct.target) ? 1 : 0;
						size_t child = make_id(dfa_id, ct.target, zidx);
						next_sets.bykey(ct.ch).push_back(child);
					}
					if (dfa->v_is_term(sub_state_id)) {
						if (b_is_final) {
							THROW_STD(invalid_argument,
								"multiple subdfa has overlap, not allowed");
						}
						m_dyndfa.m_term_submap.ensure_set(bfshead, TermSubMap{
							uint32_t(dfa_id),
							uint32_t(sub_state_id)
						});
						b_is_final = true;
					}
				}
			}
		}
		children.erase_all();
		for(size_t i = 0; i < next_sets.size(); ++i) {
			auto& ss = next_sets.byidx(i); // don't need sort unique
			size_t next;
			if (ss.size() == 1) {
				next = ss[0];
				if (get_zstr_idx(next) == 1)
					next--; // set zstr_idx as 0, this is an improvement
			} else {
				next = make_id(0, pmap.find_or_add_subset(ss), 0);
			}
			children.push_back(CharTarget<size_t>(ss.ch, next));
		}
		if (pmap.data.size() > PowerLimit) {
			THROW_STD(runtime_error, "pmap.data.size=%zd exceed PowerLimit=%zd"
					, pmap.data.size(), PowerLimit);
		}
		if (!children.empty()) {
			std::sort(children.begin(), children.end(), CharTarget_By_ch());
			m_dyndfa.resize_states(pmap.size());
			m_dyndfa.add_all_move(bfshead, children);
		}
		if (b_is_final) m_dyndfa.set_term_bit(bfshead);
	}
	if (getenv("LazyUnionDFA_showStat")) {
		fprintf(stderr, "LazyUnion.__allsub_states=%zd\n", allsub_states());
		fprintf(stderr, "LazyUnion.m_dyndfa.states=%zd\n", m_dyndfa.total_states());
	}
	if (getEnvBool("LazyUnionDFA_useDoubleArray", true)) {
		m_da_trie.build(&m_dyndfa, "DFS");
		m_dyndfa.clear();
	}
	else {
		m_dyndfa.m_term_submap.shrink_to_fit();
		m_dyndfa.shrink_to_fit();
	}
	m_subdfa_is_same_class = true;
	fstring first_class_name = typeid(*m_subdfa[1]).name();
	for (size_t i = 2; i < m_subdfa.size(); ++i) {
		fstring name2 = typeid(*m_subdfa[i]).name();
		if (name2 != first_class_name) {
			m_subdfa_is_same_class = false;
			break;
		}
	}
	m_dfa_str_bounds.reserve(m_subdfa.size());
	m_dfa_str_bounds.reserve_strpool(m_subdfa.size()*32);
	m_dfa_str_bounds.push_back(fstring());
	for (size_t i = 1; i < m_subdfa.size(); ++i) {
		ADFA_LexIteratorUP iter(m_subdfa[i]->adfa_make_iter());
		if (iter->seek_end()) {
			m_dfa_str_bounds.push_back(iter->word());
		} else {
			THROW_STD(invalid_argument,
				"build m_dfa_str_bounds: subdfa[%zd] is empty", i);
		}
	}
	m_dfa_str_bounds.shrink_to_fit();
	this->m_adfa_total_words_len = 0;
	for (size_t i = 1; i < m_subdfa.size(); ++i) {
		auto subdfa = m_subdfa[i];
		this->m_adfa_total_words_len += subdfa->adfa_total_words_len();
	}
}

// this is useless, the result sub dfa idx is not the lower bound
// or upper bound
size_t LazyUnionDFA::find_sub_dfa(fstring key) const {
	auto da = m_da_trie.m_states.data();
	size_t state = initial_state;
	for (size_t i = 0; i < key.size(); i++) {
		byte_t ch = key.p[i];
		size_t child = da[state].m_child0 + ch;
		size_t map_sub_state = da[state].m_map_sub_state;
		if (terark_unlikely(UINT32_MAX != map_sub_state))
			return da[state].m_subdfa_id - 1;
		if (terark_likely(da[child].m_parent == state))
			state = child;
		else
			break;
	}
	return m_subdfa.size()-1;
}

size_t LazyUnionDFA::v_num_children(size_t state_id) const {
	TERARK_DIE("should not call");
}

bool LazyUnionDFA::v_has_children(size_t state_id) const {
#if 0
	size_t sub_state_id = get_state_id(state_id);
	if (state_id >= max_base_state) {
		size_t dfa_id = get_subdfa_id(state_id);
		const  BaseDFA* dfa = m_subdfa[dfa_id];
		if (dfa->v_has_children(sub_state_id)) return true;
	   	else if (dfa->v_is_pzip(sub_state_id)) {
			MatchContext ctx;
			size_t zidx = get_zstr_idx(state_id);
			size_t zlen = dfa->v_get_zpath_data(sub_state_id, &ctx).size();
			return zidx <= zlen;
		}
		else return false;
	}
	else {
		return m_dyndfa.has_children(sub_state_id);
	}
#else
	THROW_STD(invalid_argument, "Method is not implemented");
#endif
}

size_t LazyUnionDFA::v_gnode_states() const {
	return m_gnode_states;
}

bool LazyUnionDFA::has_freelist() const {
	return false;
}

bool LazyUnionDFA::is_free(size_t state_id) const {
	return false;
/*
	if (state_id >= max_base_state) {
		assert(get_zstr_idx(state_id) == 0);
		size_t dfa_id = get_subdfa_id(state_id);
		size_t sub_state_id = get_state_id(state_id);
		const  BaseDFA* dfa = m_subdfa[dfa_id];
		return dfa->v_is_free(sub_state_id);
	} else
		return false;
*/
}

bool LazyUnionDFA::is_pzip(size_t state_id) const {
	if (state_id >= max_base_state) {
		size_t sub_state_id = get_state_id(state_id);
		size_t dfa_id = get_subdfa_id(state_id);
		const  BaseDFA* dfa = m_subdfa[dfa_id];
		if (dfa->v_is_pzip(sub_state_id))
			return 0 == get_zstr_idx(state_id);
		else
			return false;
	} else
		return false;
}

bool LazyUnionDFA::is_term(size_t state_id) const {
	size_t sub_state_id = get_state_id(state_id);
	if (state_id >= max_base_state) {
		size_t dfa_id = get_subdfa_id(state_id);
		const  BaseDFA* dfa = m_subdfa[dfa_id];
		size_t zidx = get_zstr_idx(state_id);
		if (zidx && dfa->v_is_pzip(sub_state_id)) {
			MatchContext ctx;
			size_t  zlen = dfa->v_get_zpath_data(sub_state_id, &ctx).size();
			if (zidx <= zlen)
				return false;
		}
		return dfa->v_is_term(sub_state_id);
	}
	if (m_da_trie.m_states.size()) {
		return m_da_trie.m_states[sub_state_id].is_term();
	} else
		return m_dyndfa.is_term(sub_state_id);
}

fstring LazyUnionDFA::get_zpath_data(size_t state_id, MatchContext* ctx)
const {
	size_t sub_state_id = get_state_id(state_id);
	size_t dfa_id = get_subdfa_id(state_id);
	size_t zidx = get_zstr_idx(state_id);
	if (state_id >= max_base_state) {
		const  BaseDFA* dfa = m_subdfa[dfa_id];
		if (dfa->v_is_pzip(sub_state_id)) {
			fstring zstr = dfa->v_get_zpath_data(sub_state_id, ctx);
			if (zidx) {
				assert(0);
				size_t zlen = zstr.size();
				THROW_STD(logic_error
					, "state_id=0x%zX is Not PathZipped: "
					  "dfa_id=0x%zX(%zd) "
					  "sub_state_id=0x%zX(%zd)"
					  "zidx=%zd zlen=%zd"
					, state_id, dfa_id, dfa_id
					, sub_state_id, sub_state_id, zidx, zlen
					);
			}
			return zstr;
		}
		else {
			assert(0 == zidx);
		}
	}
	assert(0);
	THROW_STD(logic_error, "state_id=(%zd %zd %zd) is in m_dyndfa"
			, dfa_id, sub_state_id, zidx);
}

size_t LazyUnionDFA::state_move(size_t parent, auchar_t ch) const {
	size_t sub_parent = get_state_id(parent);
	size_t child = nil_state;
	if (parent < max_base_state) {
		if (m_da_trie.m_states.size())
			child = m_da_trie.state_move(sub_parent, ch);
		else
			child = m_dyndfa.state_move(sub_parent, ch);
	} else {
		size_t dfa_id = get_subdfa_id(parent);
		assert(dfa_id < m_subdfa.size());
		const BaseDFA* dfa = m_subdfa[dfa_id];
		size_t zidx = get_zstr_idx(parent);
		if (zidx && dfa->v_is_pzip(sub_parent)) {
			MatchContext ctx;
			fstring zstr = dfa->v_get_zpath_data(sub_parent, &ctx);
			if (zidx <= zstr.size()) {
				if (zstr[zidx-1] == ch)
					child = parent + 1;
			} else
				goto SimpleState;
		} else { SimpleState:
			size_t child2 = dfa->v_state_move(sub_parent, ch);
			if (dfa->v_nil_state() != child2)
				child = make_id(dfa_id, child2, 0);
		}
	}
	return child;
}

void LazyUnionDFA::for_each_move(size_t parent, const OnMove& on_move)
const {
	size_t sub_parent = get_state_id(parent);
	if (parent < max_base_state) {
		if (m_da_trie.m_states.size())
			m_da_trie.for_each_move<const OnMove&>(sub_parent, on_move);
		else
			m_dyndfa.for_each_move(sub_parent, &on_move);
	} else {
		size_t dfa_id = get_subdfa_id(parent);
		assert(dfa_id < m_subdfa.size());
		const BaseDFA* dfa = m_subdfa[dfa_id];
		size_t zidx = get_zstr_idx(parent);
		if (zidx && dfa->v_is_pzip(sub_parent)) {
			MatchContext ctx;
			fstring zstr = dfa->v_get_zpath_data(sub_parent, &ctx);
			if (zidx <= zstr.size())
				on_move(parent + 1, zstr[zidx-1]);
			else
				goto SimpleState;
		} else { SimpleState:
			auto on_move_n = [&](size_t child, auchar_t ch) {
				size_t  global_child = make_id(dfa_id, child, 0);
				on_move(global_child, ch);
			};
			dfa->v_for_each_move(sub_parent, ref(on_move_n));
		}
	}
}

void LazyUnionDFA::for_each_dest(size_t parent, const OnDest& on_dest)
const {
	size_t sub_parent = get_state_id(parent);
	if (parent < max_base_state) {
		if (m_da_trie.m_states.size())
			m_da_trie.for_each_dest<const OnDest&>(sub_parent, on_dest);
		else
			m_dyndfa.for_each_dest(sub_parent, &on_dest);
	} else {
		size_t dfa_id = get_subdfa_id(parent);
		assert(dfa_id < m_subdfa.size());
		const BaseDFA* dfa = m_subdfa[dfa_id];
		size_t zidx = get_zstr_idx(parent);
		if (zidx && dfa->v_is_pzip(sub_parent)) {
			MatchContext ctx;
			fstring zstr = dfa->v_get_zpath_data(sub_parent, &ctx);
			if (zidx <= zstr.size())
				on_dest(parent + 1);
			else
				goto SimpleState;
		} else { SimpleState:
			auto on_dest_n = [&](size_t child) {
				size_t  global_child = make_id(dfa_id, child, 0);
				on_dest(global_child);
			};
			dfa->v_for_each_dest(sub_parent, ref(on_dest_n));
		}
	}
}

void LazyUnionDFA::for_each_dest_rev(size_t parent, const OnDest& on_dest)
const {
	size_t sub_parent = get_state_id(parent);
	if (parent < max_base_state) {
		if (m_da_trie.m_states.size())
			m_da_trie.for_each_dest_rev<const OnDest&>(sub_parent, on_dest);
		else
			m_dyndfa.for_each_dest_rev(sub_parent, &on_dest);
	} else {
		size_t dfa_id = get_subdfa_id(parent);
		assert(dfa_id < m_subdfa.size());
		const BaseDFA* dfa = m_subdfa[dfa_id];
		size_t zidx = get_zstr_idx(parent);
		if (zidx && dfa->v_is_pzip(sub_parent)) {
			MatchContext ctx;
			fstring zstr = dfa->v_get_zpath_data(sub_parent, &ctx);
			if (zidx <= zstr.size())
				on_dest(parent + 1);
			else
				goto SimpleState;
		} else { SimpleState:
			auto on_dest_n = [&](size_t child) {
				size_t  global_child = make_id(dfa_id, child, 0);
				on_dest(global_child);
			};
			dfa->v_for_each_dest_rev(sub_parent, ref(on_dest_n));
		}
	}
}

size_t LazyUnionDFA::
for_each_suffix(MatchContext& ctx, fstring prefix, const OnNthWord& f)
const {
	size_t curr = ctx.root;
	auto da = m_da_trie.m_states.data();
	for (size_t i = ctx.pos; ; ++i) {
		size_t curr_id = get_state_id(curr);
		if (curr >= max_base_state) {
			size_t dfa_id = get_subdfa_id(curr);
			size_t zidx = get_zstr_idx(curr);
			const MatchingDFA* dfa = m_subdfa[dfa_id];
			assert(dfa_id < dfa->v_total_states());
			ctx.root = curr_id;
			ctx.zidx = zidx ? zidx-1 : 0;
			ctx.pos = i;
			return dfa->for_each_suffix(ctx, prefix, f);
		}
		assert(i <= prefix.size());
		if (prefix.size() == i)
			break;
		if (da) {
			size_t next = da[curr_id].child0() + byte_t(prefix.p[i]);
			if (da[next].parent() == curr_id) {
				curr = next;
				continue;
			} else
				return 0;
		}
		size_t next = m_dyndfa.state_move(curr_id, byte_t(prefix.p[i]));
		if (nil_state == next)
			return 0;
		curr = next;
	}
	auto& forEachWord = NonRecursiveForEachWord::cache(ctx);
	return forEachWord(*this, curr, 0, DFA_OnWord3(f));
}

namespace {
	typedef AcyclicPathDFA::OnMatchKey OnMatchKey;
	static inline size_t
	match_key_sub(const MatchingDFA* dfa, MatchContext& ctx, auchar_t delim,
				  fstring str, const OnMatchKey& f, IdentityTR) {
		return dfa->match_key(ctx, delim, str, f);
	}
	static inline size_t
	match_key_sub(const MatchingDFA* dfa, MatchContext& ctx, auchar_t delim,
				  fstring str, const OnMatchKey& f, const ByteTR& tr) {
		return dfa->match_key(ctx, delim, str, f, tr);
	}
	static inline size_t
	match_key_sub(const MatchingDFA* dfa, MatchContext& ctx, auchar_t delim,
				  ByteInputRange& str, const OnMatchKey& f, IdentityTR) {
		return dfa->match_key(ctx, delim, str, f);
	}
	static inline size_t
	match_key_sub(const MatchingDFA* dfa, MatchContext& ctx, auchar_t delim,
				  ByteInputRange& str, const OnMatchKey& f, const ByteTR& tr) {
		return dfa->match_key(ctx, delim, str, f, tr);
	}
	static inline size_t
	match_key_l_sub(const MatchingDFA* dfa, MatchContext& ctx, auchar_t delim,
					fstring str, const OnMatchKey& f, IdentityTR) {
		return dfa->match_key_l(ctx, delim, str, f);
	}
	static inline size_t
	match_key_l_sub(const MatchingDFA* dfa, MatchContext& ctx, auchar_t delim,
					fstring str, const OnMatchKey& f, const ByteTR& tr) {
		return dfa->match_key_l(ctx, delim, str, f, tr);
	}
	static inline size_t
	match_key_l_sub(const MatchingDFA* dfa, MatchContext& ctx, auchar_t delim,
					ByteInputRange& str, const OnMatchKey& f, IdentityTR) {
		return dfa->match_key_l(ctx, delim, str, f);
	}
	static inline size_t
	match_key_l_sub(const MatchingDFA* dfa, MatchContext& ctx, auchar_t delim,
					ByteInputRange& str, const OnMatchKey& f, const ByteTR& tr) {
		return dfa->match_key_l(ctx, delim, str, f, tr);
	}

	static inline bool
	step_key_sub(const MatchingDFA* dfa, MatchContext& ctx,
				 auchar_t delim, fstring str, IdentityTR) {
		return dfa->step_key(ctx, delim, str);
	}
	static inline bool
	step_key_sub(const MatchingDFA* dfa, MatchContext& ctx,
				 auchar_t delim, fstring str, const ByteTR& tr) {
		return dfa->step_key(ctx, delim, str, tr);
	}
}

class LazyUnionDFA::PrefixDFA {
	const LazyUnionDFA* m_dfa;
public:
	enum { sigma = 256 };
	size_t total_states() const { return m_dfa->total_states(); }
	size_t num_zpath_states() const { return 0; }
	size_t total_zpath_len() const { return 0; }
	PrefixDFA(const LazyUnionDFA* dfa) : m_dfa(dfa) {}
	template<class OP>
	void for_each_move(size_t parent, OP op) const {
		if (parent < max_base_state) {
			assert(get_zstr_idx(parent) == 0);
			size_t sub_parent = get_state_id(parent);
			if (m_dfa->m_da_trie.m_states.size())
				m_dfa->m_da_trie.for_each_move<OP>(sub_parent, op);
			else
				m_dfa->m_dyndfa.for_each_move<OP>(sub_parent, op);
		}
	}
	bool is_term(size_t state) const {
		if (state >= max_base_state)
			return true;
		size_t sub_state = get_state_id(state);
		if (m_dfa->m_da_trie.m_states.size())
			return m_dfa->m_da_trie.m_states[sub_state].is_term();
		else
			return m_dfa->m_dyndfa.is_term(sub_state);
	}
	fstring get_zpath_data(size_t, MatchContext*) const {
		THROW_STD(logic_error, "this function should not be called");
	}
	bool is_pzip(size_t) const { return false; }
};
class LazyUnionDFA::ForEachValueOnPrefix {
public:
	const LazyUnionDFA* m_dfa;
	size_t m_base_nth;
	size_t m_keylen;
	const OnMatchKey&   m_func;

	/// @param prefix prefix in m_dyndfa of whole string
	void operator()(size_t nth, fstring prefix, size_t state) {
		size_t state_id = get_state_id(state);
		if (state < max_base_state) {
#if !defined(NDEBUG)
			if (m_dfa->m_da_trie.m_states.size()) {
				assert(state_id < m_dfa->m_da_trie.m_states.size());
				assert(m_dfa->m_da_trie.m_states[state_id].is_term());
			} else {
				assert(state_id < m_dfa->m_dyndfa.total_states());
				assert(m_dfa->m_dyndfa.is_term(state_id));
			}
#endif
		//	printf("state=%zd < max_base_state=%zd\n", state, max_base_state);
			m_func(prefix.size(), m_base_nth, prefix);
			m_base_nth++;
		} else {
			size_t dfa_id = get_subdfa_id(state);
			size_t zidx = get_zstr_idx(state);
			const MatchingDFA* subdfa = m_dfa->m_subdfa[dfa_id];
		//	printf("state=%zd >= max_base_state=%zd dfa_id=%zd zidx=%zd\n",
		//	state, max_base_state, dfa_id, zidx);
			if (zidx) {
				zidx--;
				assert(subdfa->v_is_pzip(state_id));
			}
			m_base_nth += subdfa->prepend_for_each_value(
					m_keylen, m_base_nth, prefix, state_id, zidx, m_func);
		}
	}
	ForEachValueOnPrefix(const LazyUnionDFA* dfa, size_t keylen, const OnMatchKey& f)
		: m_dfa(dfa), m_base_nth(0), m_keylen(keylen), m_func(f) {}
};
class LazyUnionDFA::ForEachWordOnPrefix {
	const LazyUnionDFA* m_dfa;
	const OnNthWord&    m_func;
	size_t m_base_nth;
public:
	void operator()(size_t nth, fstring prefix, size_t state) {
		size_t state_id = get_state_id(state);
		if (state < max_base_state) {
#if !defined(NDEBUG)
			if (m_dfa->m_da_trie.m_states.size()) {
				assert(state_id < m_dfa->m_da_trie.m_states.size());
				assert(m_dfa->m_da_trie.m_states[state_id].is_term());
			} else {
				assert(state_id < m_dfa->m_dyndfa.total_states());
				assert(m_dfa->m_dyndfa.is_term(state_id));
			}
#endif
			m_func(m_base_nth, prefix);
			m_base_nth++;
		} else {
			size_t dfa_id = get_subdfa_id(state);
			size_t zidx = get_zstr_idx(state);
			const MatchingDFA* subdfa = m_dfa->m_subdfa[dfa_id];
			if (zidx) {
				zidx--;
				assert(subdfa->v_is_pzip(state_id));
			}
			m_base_nth += subdfa->prepend_for_each_word(
					m_base_nth, prefix, state_id, zidx, m_func);
		}
	}
	ForEachWordOnPrefix(const LazyUnionDFA* dfa, const OnNthWord& f)
		: m_dfa(dfa), m_func(f), m_base_nth(0) {}
};

size_t LazyUnionDFA::
for_each_value(MatchContext& ctx, const OnMatchKey& f)
const {
	assert(0 == ctx.zidx);
	if (ctx.root >= max_base_state) {
		size_t root_id = get_state_id(ctx.root);
		size_t dfa_id = get_subdfa_id(ctx.root);
		size_t zidx = get_zstr_idx(ctx.root);
		const MatchingDFA* dfa = m_subdfa[dfa_id];
		ctx.root = root_id;
		ctx.zidx = zidx ? zidx-1 : 0;
	//	printf("dfa_id=%zd ctx[pos=%zd root=%zd zidx=%zd]\n",
	//	dfa_id, ctx.pos, ctx.root, ctx.zidx);
		return dfa->for_each_value(ctx, f);
	} else {
		auto& forEachWord = NonRecursiveForEachWord::cache(ctx);
		return forEachWord(PrefixDFA(this),
			ctx.root, 0, ForEachValueOnPrefix(this, ctx.pos, f));
	}
}

size_t LazyUnionDFA::
for_each_word(size_t root, size_t zidx, const OnNthWord& f)
const {
	assert(0 == zidx);
	if (root >= max_base_state) {
		size_t root_id = get_state_id(root);
		size_t dfa_id = get_subdfa_id(root);
		size_t realzidx = get_zstr_idx(root);
		const MatchingDFA* dfa = m_subdfa[dfa_id];
		if (realzidx) realzidx--;
		return dfa->for_each_word(root_id, realzidx, f);
	} else {
		MatchContext ctx;
		NonRecursiveForEachWord forEachWord(&ctx);
		return forEachWord(PrefixDFA(this),
			root, 0, ForEachWordOnPrefix(this, f));
	}
}

size_t LazyUnionDFA::prepend_for_each_word_ex
(size_t, fstring, size_t, size_t, const OnNthWordEx&)
const {
	THROW_STD(logic_error, "this function should not be called");
}

size_t LazyUnionDFA::prepend_for_each_value
(size_t, size_t, fstring, size_t, size_t, const OnMatchKey&)
const {
	THROW_STD(logic_error, "this function should not be called");
}
size_t LazyUnionDFA::prepend_for_each_word
(size_t, fstring, size_t, size_t, const OnNthWord&)
const {
	THROW_STD(logic_error, "this function should not be called");
}

template<class TR>
size_t LazyUnionDFA::tpl_match_key
(MatchContext& ctx, auchar_t delim, fstring str, const OnMatchKey& f, TR tr)
const {
	assert(delim < m_real_sigma);
	auto& forEachWord = NonRecursiveForEachWord::cache(ctx);
	ForEachValueOnPrefix onWord(this, -1, f);
	auto da = m_da_trie.m_states.data();
	size_t curr = ctx.root;
	size_t i = ctx.pos;
	for(; nil_state != curr; ++i) {
		size_t curr_id = get_state_id(curr);
		if (curr >= max_base_state) {
			size_t dfa_id = get_subdfa_id(curr);
			size_t zidx = get_zstr_idx(curr);
			const MatchingDFA* dfa = m_subdfa[dfa_id];
			ctx.pos = i;
			ctx.root = curr_id;
			ctx.zidx = zidx ? zidx-1 : 0;
			return match_key_sub(dfa, ctx, delim, str, f, tr);
		}
		if (da) {
			size_t valroot = da[curr_id].child0() + delim;
			if (da[valroot].parent() == curr_id) {
				onWord.m_keylen = i;
				size_t full_valroot = m_da_trie.get_full_state(da, valroot);
				forEachWord(PrefixDFA(this), full_valroot, 0, onWord);
			}
			if (str.size() == i) break;
			size_t child = da[curr_id].child0() + (byte_t)tr(str[i]);
			if (da[child].parent() == curr_id) {
				curr = m_da_trie.get_full_state(da, child);
				continue;
			} else
				break;
		}
		size_t valroot = m_dyndfa.state_move(curr_id, delim);
		if (nil_state != valroot) {
			onWord.m_keylen = i;
			forEachWord(PrefixDFA(this), valroot, 0, onWord);
		}
		if (str.size() == i) break;
		curr = m_dyndfa.state_move(curr_id, (byte_t)tr(str[i]));
	}
	return i;
}

template<class TR>
size_t LazyUnionDFA::stream_match_key
(MatchContext& ctx, auchar_t delim, ByteInputRange& r, const OnMatchKey& f, TR tr)
const {
	assert(delim < m_real_sigma);
	auto& forEachWord = NonRecursiveForEachWord::cache(ctx);
	ForEachValueOnPrefix onWord(this, -1, f);
	auto da = m_da_trie.m_states.data();
	size_t curr = ctx.root;
	size_t i = ctx.pos;
	for (; nil_state != curr; ++i, ++r) {
		size_t curr_id = get_state_id(curr);
		if (curr >= max_base_state) {
			size_t dfa_id = get_subdfa_id(curr);
			size_t zidx = get_zstr_idx(curr);
			const MatchingDFA* dfa = m_subdfa[dfa_id];
			ctx.pos = i;
			ctx.root = curr_id;
			ctx.zidx = zidx ? zidx-1 : 0;
			return match_key_sub(dfa, ctx, delim, r, f, tr);
		}
		if (da) {
			size_t valroot = da[curr_id].child0() + delim;
			if (da[valroot].parent() == curr_id) {
				onWord.m_keylen = i;
				size_t full_valroot = m_da_trie.get_full_state(da, valroot);
				forEachWord(PrefixDFA(this), full_valroot, 0, onWord);
			}
			if (r.empty()) break;
			size_t child = da[curr_id].child0() + (byte_t)tr(*r);
			if (da[child].parent() == curr_id) {
				curr = m_da_trie.get_full_state(da, child);
				continue;
			} else
				break;
		}
		size_t valroot = m_dyndfa.state_move(curr_id, delim);
		if (nil_state != valroot) {
			onWord.m_keylen = i;
			forEachWord(PrefixDFA(this), valroot, 0, onWord);
		}
		if (r.empty()) break;
		curr = m_dyndfa.state_move(curr_id, (byte_t)tr(*r));
	}
	return i;
}

template<class TR>
size_t LazyUnionDFA::tpl_match_key_l
(MatchContext& ctx, auchar_t delim, fstring str, const OnMatchKey& f, TR tr)
const {
	auto da = m_da_trie.m_states.data();
	size_t curr = ctx.root;
	size_t i = ctx.pos;
	size_t last_state = nil_state;
	size_t last_i = 0;
	for (; nil_state != curr; ++i) {
		size_t curr_id = get_state_id(curr);
		if (curr >= max_base_state) {
			size_t dfa_id = get_subdfa_id(curr);
			size_t zidx = get_zstr_idx(curr);
			const MatchingDFA* dfa = m_subdfa[dfa_id];
			ctx.pos = i;
			ctx.root = curr_id;
			ctx.zidx = zidx ? zidx-1 : 0;
			return match_key_l_sub(dfa, ctx, delim, str, f, tr);
		}
		if (da) {
			size_t valroot = da[curr_id].child0() + delim;
			if (da[valroot].parent() == curr_id) {
				last_i = i;
				last_state = m_da_trie.get_full_state(da, valroot);
			}
			if (str.size() == i) break;
			size_t child = da[curr_id].child0() + (byte_t)tr(str[i]);
			if (da[child].parent() == curr_id) {
				curr = m_da_trie.get_full_state(da, child);
				continue;
			} else
				break;
		}
		size_t valroot = m_dyndfa.state_move(curr_id, delim);
		if (nil_state != valroot) {
			last_i = i;
			last_state = valroot;
		}
		if (str.size() == i) break;
		curr = m_dyndfa.state_move(curr_id, (byte_t)tr(byte_t(str[i])));
	}
	if (nil_state != last_state) {
		auto& forEachWord = NonRecursiveForEachWord::cache(ctx);
		ForEachValueOnPrefix onWord(this, last_i, f);
		forEachWord(PrefixDFA(this), last_state, 0, onWord);
	}
	return i;
}

template<class TR>
size_t LazyUnionDFA::stream_match_key_l
(MatchContext& ctx, auchar_t delim, ByteInputRange& r, const OnMatchKey& f, TR tr)
const {
	assert(delim < sigma);
	auto da = m_da_trie.m_states.data();
	size_t curr = ctx.root;
	size_t i = ctx.pos;
	size_t last_state = nil_state;
	size_t last_i = 0;
	for (; nil_state != curr; ++i, ++r) {
		size_t curr_id = get_state_id(curr);
		if (curr >= max_base_state) {
			size_t dfa_id = get_subdfa_id(curr);
			size_t zidx = get_zstr_idx(curr);
			const MatchingDFA* dfa = m_subdfa[dfa_id];
			ctx.pos = i;
			ctx.root = curr_id;
			ctx.zidx = zidx ? zidx-1 : 0;
			return match_key_l_sub(dfa, ctx, delim, r, f, tr);
		}
		if (da) {
			size_t valroot = da[curr_id].child0() + delim;
			if (da[valroot].parent() == curr_id) {
				last_i = i;
				last_state = m_da_trie.get_full_state(da, valroot);
			}
			if (r.empty()) break;
			size_t child = da[curr_id].child0() + (byte_t)tr(*r);
			if (da[child].parent() == curr_id) {
				curr = m_da_trie.get_full_state(da, child);
				continue;
			} else
				break;
		}
		size_t valroot = m_dyndfa.state_move(curr_id, delim);
		if (nil_state != valroot) {
			last_i = i;
			last_state = valroot;
		}
		if (r.empty()) break;
		curr = m_dyndfa.state_move(curr_id, (byte_t)tr(*r));
	}
	if (nil_state != last_state) {
		auto& forEachWord = NonRecursiveForEachWord::cache(ctx);
		ForEachValueOnPrefix onWord(this, last_i, f);
		forEachWord(PrefixDFA(this), last_state, 0, onWord);
	}
	return i;
}

size_t LazyUnionDFA::match_key
(MatchContext& ctx, auchar_t delim, fstring str, const OnMatchKey& f)
const {
	return tpl_match_key<IdentityTR>(ctx, delim, str, f, IdentityTR());
}
size_t LazyUnionDFA::match_key
(MatchContext& ctx, auchar_t delim, fstring str, const OnMatchKey& f, const ByteTR& tr)
const {
	return tpl_match_key<const ByteTR&>(ctx, delim, str, f, tr);
}

size_t LazyUnionDFA::match_key
(MatchContext& ctx, auchar_t delim, ByteInputRange& r, const OnMatchKey& f)
const {
	return stream_match_key(ctx, delim, r, f, IdentityTR());
}
size_t LazyUnionDFA::match_key
(MatchContext& ctx, auchar_t delim, ByteInputRange& r, const OnMatchKey& f, const ByteTR& tr)
const {
	return stream_match_key<const ByteTR&>(ctx, delim, r, f, tr);
}

size_t LazyUnionDFA::match_key_l
(MatchContext& ctx, auchar_t delim, fstring str, const OnMatchKey& f)
const {
	return tpl_match_key_l(ctx, delim, str, f, IdentityTR());
}
size_t LazyUnionDFA::match_key_l
(MatchContext& ctx, auchar_t delim, fstring str, const OnMatchKey& f, const ByteTR& tr)
const {
	return tpl_match_key_l<const ByteTR&>(ctx, delim, str, f, tr);
}
size_t LazyUnionDFA::match_key_l
(MatchContext& ctx, auchar_t delim, ByteInputRange& r, const OnMatchKey& f)
const {
	return stream_match_key_l(ctx, delim, r, f, IdentityTR());
}
size_t LazyUnionDFA::match_key_l
(MatchContext& ctx, auchar_t delim, ByteInputRange& r, const OnMatchKey& f, const ByteTR& tr)
const {
	return stream_match_key_l<const ByteTR&>(ctx, delim, r, f, tr);
}

bool LazyUnionDFA::
step_key(MatchContext& ctx, auchar_t delim, fstring str)
const {
	return tpl_step_key(ctx, delim, str, IdentityTR());
}

bool LazyUnionDFA::
step_key(MatchContext& ctx, auchar_t delim, fstring str, const ByteTR& tr)
const {
	return tpl_step_key(ctx, delim, str, tr);
}

bool LazyUnionDFA::
step_key_l(MatchContext& ctx, auchar_t delim, fstring str)
const {
	return tpl_step_key_l(ctx, delim, str, IdentityTR());
}

bool LazyUnionDFA::
step_key_l(MatchContext& ctx, auchar_t delim, fstring str, const ByteTR& tr)
const {
	return tpl_step_key_l(ctx, delim, str, tr);
}

template<class TR>
bool LazyUnionDFA::
tpl_step_key(MatchContext& ctx, auchar_t delim, fstring str, TR tr)
const {
	auto da = m_da_trie.m_states.data();
	size_t parent = ctx.root;
	size_t pos2 = ctx.pos;
	if (pos2 >= str.size()) {
		THROW_STD(invalid_argument,
				"strpos=%zd >= str.size=%zd", pos2, str.size());
	}
	for(; ; ++pos2) {
		assert(pos2 <= str.size());
		size_t parent_id = get_state_id(parent);
		if (parent >= max_base_state) {
			size_t dfa_id = get_subdfa_id(parent);
			size_t zidx = get_zstr_idx(parent);
			const MatchingDFA* dfa = m_subdfa[dfa_id];
			assert(dfa_id < dfa->v_total_states());
			ctx.root = parent_id;
			ctx.zidx = zidx ? zidx-1 : 0;
			ctx.pos = pos2;
		//	printf("calling step_key_sub: ctx[pos=%zd root=%zd zidx=%zd]\n", ctx.pos, ctx.root, ctx.zidx);
			bool ret = step_key_sub(dfa, ctx, delim, str, tr);
			ctx.root = make_id(dfa_id, ctx.root, ctx.zidx ? ctx.zidx+1 : 0);
			ctx.zidx = 0;
			return ret;
		}
		if (da) {
			size_t valroot = da[parent_id].child0() + delim;
			if (da[valroot].parent() == parent_id) {
				if (str.size() == pos2 || (uint08_t)tr(str[pos2]) == delim) {
					ctx.root = m_da_trie.get_full_state(da, valroot);
					ctx.zidx = 0;
					ctx.pos = pos2 + 1;
					return true;
				}
			}
			else if (str.size() == pos2) {
				ctx.root = make_id(0, parent_id, 0);
				ctx.zidx = 0;
				ctx.pos = pos2;
				return false;
			}
			size_t child = da[parent_id].child0() + (uint08_t)tr(str[pos2]);
			if (da[child].parent() != parent_id) {
				ctx.root = make_id(0, parent_id, 0);
				ctx.zidx = 0;
				ctx.pos = pos2;
				return false;
			}
			parent = m_da_trie.get_full_state(da, child);
			continue;
		}
		size_t valroot = m_dyndfa.state_move(parent_id, delim);
		if (nil_state != valroot) {
			if (str.size() == pos2 || (uint08_t)tr(str[pos2]) == delim) {
				ctx.root = valroot;
				ctx.zidx = 0;
				ctx.pos = pos2 + 1;
				return true;
			}
		}
		else if (str.size() == pos2) {
			ctx.root = make_id(0, parent_id, 0);
			ctx.zidx = 0;
			ctx.pos = pos2;
			return false;
		}
		size_t child = m_dyndfa.state_move(parent_id, (uint08_t)tr(str[pos2]));
		if (nil_state == child) {
			ctx.root = make_id(0, parent_id, 0);
			ctx.zidx = 0;
			ctx.pos = pos2;
			return false;
		}
		parent = child;
	}
	return false; // never get here...
}

template<class TR>
bool LazyUnionDFA::
tpl_step_key_l(MatchContext& ctx, auchar_t delim, fstring str, TR tr)
const {
	auto da = m_da_trie.m_states.data();
	size_t parent = ctx.root;
	size_t pos0 = ctx.pos;
	size_t pos2 = ctx.pos;
	if (pos2 >= str.size()) {
		THROW_STD(invalid_argument,
				"strpos=%zd >= str.size=%zd", pos2, str.size());
	}
	for(; ; ++pos2) {
		assert(pos2 <= str.size());
		size_t parent_id = get_state_id(parent);
		if (parent >= max_base_state) {
			size_t dfa_id = get_subdfa_id(parent);
			size_t zidx = get_zstr_idx(parent);
			const MatchingDFA* dfa = m_subdfa[dfa_id];
			assert(dfa_id < dfa->v_total_states());
			ctx.root = parent_id;
			ctx.zidx = zidx ? zidx-1 : 0;
			ctx.pos = pos2;
			bool ret = step_key_sub(dfa, ctx, delim, str, tr);
			ctx.root = make_id(dfa_id, ctx.root, ctx.zidx ? ctx.zidx+1 : 0);
			ctx.zidx = 0;
			return ret;
		}
		if (da) {
			size_t valroot = da[parent_id].child0() + delim;
			if (da[valroot].parent() == parent_id) {
				size_t full_valroot = m_da_trie.get_full_state(da, valroot);
				if (str.size() == pos2) {
					ctx.root = full_valroot;
					ctx.zidx = 0;
					ctx.pos  = pos2 + 1;
					return true;
				}
				else if ((uint08_t)tr(str[pos2]) == delim) {
					ctx.root = full_valroot;
					ctx.zidx = 0;
					ctx.pos  = pos2 + 1;
				}
			}
			else if (str.size() == pos2) {
				goto Done;
			}
			size_t child = da[parent_id].child0() + (uint08_t)tr(str[pos2]);
			if (da[child].parent() != parent_id) {
				goto Done;
			}
			parent = m_da_trie.get_full_state(da, child);
			continue;
		}
		size_t valroot = m_dyndfa.state_move(parent_id, delim);
		if (nil_state != valroot) {
			if (str.size() == pos2) {
				ctx.root = valroot;
				ctx.zidx = 0;
				ctx.pos  = pos2 + 1;
				return true;
			}
			else if ((uint08_t)tr(str[pos2]) == delim) {
				ctx.root = valroot;
				ctx.zidx = 0;
				ctx.pos  = pos2 + 1;
			}
		}
		else if (str.size() == pos2) goto Done;
		size_t child = m_dyndfa.state_move(parent_id, (byte_t)tr(str[pos2]));
		if (nil_state == child) goto Done;
		parent = child;
	}
	return false; // never get here...
Done:
	if (pos0 < ctx.pos) {
		return true;
	} else {
		ctx.root = parent;
		ctx.zidx = 0;
		ctx.pos = pos2;
		return false;
	}
}

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

const BaseDAWG* LazyUnionDAWG::get_dawg() const { return this; }
const SuffixCountableDAWG* LazyUnionDAWG::get_SuffixCountableDAWG()
const {
	if (this->m_has_suffix_cnt)
		return this;
	else
		return NULL;
}

void LazyUnionDAWG::compile(size_t PowerLimit) {
	this->n_words = 0;
	this->m_has_suffix_cnt = true;
	this->m_word_num_vec.resize_no_init(m_subdfa.size());
	this->m_word_num_vec[0] = 0;
	for(size_t i = 1; i < m_subdfa.size(); ++i) {
		this->n_words += m_subdfa[i]->get_dawg()->n_words;
		m_word_num_vec[i] = this->n_words;
		m_has_suffix_cnt &= m_subdfa[i]->get_SuffixCountableDAWG() != NULL;
	}

	LazyUnionDFA::compile(PowerLimit);

	// now compute suffix_cnt and DAWG Target::num;
	for(size_t i = 0; i < m_dyndfa.states.size(); ++i)
		m_dyndfa.states[i].n_suffix = 0;

	if (m_has_suffix_cnt)
		compile_loop(initial_state);
}

void LazyUnionDAWG::compile_loop(size_t parent) {
	auto& states = m_dyndfa.states;
	auto& pool   = m_dyndfa.pool;
	auto& s = states[parent];
	if (s.n_suffix)
		return; // visited
	size_t num = 0;
	if (s.more_than_one_child()) {
		auto& hb = pool.at<FastDynaDFA::header_max>(s.getpos());
		int bits = s.rlen(), nt = hb.popcnt_aligned(bits);
		auto  pt = hb.base(bits);
		for (int i = 0; i < nt; ++i) {
			size_t target = pt[i].target;
			size_t child_id = get_state_id(target);
			size_t child_cnt;
			pt[i].num = num;
			if (target >= max_base_state) {
				size_t dfa_id = get_subdfa_id(target);
				const MatchingDFA* dfa = m_subdfa[dfa_id];
				child_cnt = dfa->get_SuffixCountableDAWG()->suffix_cnt(child_id);
			} else {
				compile_loop(child_id);
				child_cnt = states[child_id].n_suffix;
			}
			num += child_cnt;
		}
	} else {
		size_t target = m_dyndfa.single_target(parent);
		size_t child_id = get_state_id(target);
		if (nil_state == target) {
			assert(0&&"unexpected");
		}
		if (target >= max_base_state) {
			size_t dfa_id = get_subdfa_id(target);
			const MatchingDFA* dfa = m_subdfa[dfa_id];
			num = dfa->get_SuffixCountableDAWG()->suffix_cnt(child_id);
		} else {
			compile_loop(child_id);
			num = states[child_id].n_suffix;
		}
	}
	s.n_suffix = num + (s.is_term() ? 1 : 0);
}

size_t LazyUnionDAWG::suffix_cnt(size_t root) const {
	if (!m_has_suffix_cnt) {
		THROW_STD(invalid_argument,
			"Current LazyUnionDAWG has no suffix count");
	}
	size_t real_id = get_state_id(root);
	if (root >= max_base_state) {
		size_t dfa_id = get_subdfa_id(root);
		const MatchingDFA* dfa = m_subdfa[dfa_id];
		assert(dfa_id < dfa->v_total_states());
		return dfa->get_SuffixCountableDAWG()->suffix_cnt(root);
	} else {
		assert(real_id < m_dyndfa.states.size());
		return m_dyndfa.states[real_id].n_suffix;
	}
}

size_t LazyUnionDAWG::
suffix_cnt(MatchContext& ctx, fstring exact_prefix) const {
	return tpl_suffix_cnt(ctx, exact_prefix, IdentityTR());
}
size_t LazyUnionDAWG::
suffix_cnt(MatchContext& ctx, fstring exact_prefix, const ByteTR& tr)
const {
	return tpl_suffix_cnt<const ByteTR&>(ctx, exact_prefix, tr);
}
size_t LazyUnionDAWG::
suffix_cnt(MatchContext& ctx, fstring exact_prefix, const byte_t* tr)
const {
	return tpl_suffix_cnt(ctx, exact_prefix, TableTranslator(tr));
}

void LazyUnionDAWG::
path_suffix_cnt(MatchContext& ctx, fstring str, valvec<size_t>* cnt)
const {
	this->tpl_path_suffix_cnt(ctx, str, cnt, IdentityTR());
}

void LazyUnionDAWG::
path_suffix_cnt(MatchContext& ctx, fstring str, valvec<size_t>* cnt, const ByteTR& tr)
const {
	this->tpl_path_suffix_cnt<const ByteTR&>(ctx, str, cnt, tr);
}

void LazyUnionDAWG::
path_suffix_cnt(MatchContext& ctx, fstring str, valvec<size_t>* cnt, const byte_t* tr)
const {
	assert(NULL != tr);
	this->tpl_path_suffix_cnt(ctx, str, cnt, TableTranslator(tr));
}

template<class TR>
size_t LazyUnionDAWG::
tpl_suffix_cnt(MatchContext& ctx, fstring str, TR tr) const {
	size_t curr = ctx.root;
	size_t num1 = suffix_cnt(curr);
	size_t i = ctx.pos;
	for(; ; ++i) {
		size_t curr_id = get_state_id(curr);
		if (curr >= max_base_state) {
			size_t dfa_id = get_subdfa_id(curr);
			size_t zidx = get_zstr_idx(curr);
			const MatchingDFA* dfa = m_subdfa[dfa_id];
			assert(dfa_id < dfa->v_total_states());
			ctx.root = curr_id;
			ctx.zidx = zidx ? zidx-1 : 0;
			ctx.pos = i;
			return suffix_cnt_sub(dfa->get_SuffixCountableDAWG(), ctx, str, tr);
		}
		assert(curr_id < m_dyndfa.states.size());
		size_t num2 = m_dyndfa.states[curr_id].n_suffix;
		assert(num2 <= num1);
		num1 = num2;
		if (str.size() == i)
			break;
		curr = this->state_move(curr_id, (byte_t)tr((byte_t)str.p[i]));
		if (nil_state == curr) {
			num1 = 0;
			break;
		}
	}
	ctx.pos = i;
	ctx.root = curr;
	ctx.zidx = get_zstr_idx(curr);
	return num1;
}

template<class TR>
void LazyUnionDAWG::
tpl_path_suffix_cnt(MatchContext& ctx, fstring str, valvec<size_t>* cnt, TR tr)
const {
	assert(ctx.pos <= str.size());
	cnt->resize_no_init(str.size()+1);
	size_t* pcnt = cnt->data();
	size_t  curr = ctx.root;
	size_t  i = ctx.pos;
	for (; nil_state != curr; ++i) {
		size_t curr_id = get_state_id(curr);
		if (curr >= max_base_state) {
			size_t dfa_id = get_subdfa_id(curr);
			size_t zidx = get_zstr_idx(curr);
			const MatchingDFA* dfa = m_subdfa[dfa_id];
			assert(dfa_id < dfa->v_total_states());
			ctx.root = curr_id;
			ctx.zidx = zidx ? zidx-1 : 0;
			ctx.pos = i;
			path_suffix_cnt_sub(dfa->get_SuffixCountableDAWG(), ctx, str, cnt, tr);
			return;
		}
		assert(curr_id < m_dyndfa.states.size());
		pcnt[i] = m_dyndfa.states[curr_id].n_suffix;
		if (str.size() == i) break;
		curr = this->state_move(curr_id, (byte_t)tr((byte_t)str.p[i]));
	}
	ctx.pos = i;
	ctx.root = curr;
	ctx.zidx = nil_state == curr ? 0 : get_zstr_idx(curr);
	cnt->risk_set_size(i+1);
}

template<class TR>
size_t LazyUnionDAWG::
tpl_match_dawg(MatchContext& ctx, size_t base_nth, fstring str, const OnMatchDAWG& f, TR tr)
const {
#if !defined(NDEBUG)
//	size_t root_num = suffix_cnt(ctx.root);
#endif
	if (auto da = m_da_trie.m_states.data()) {
		size_t curr = ctx.root;
		size_t idx = 0;
		size_t i = ctx.pos;
		for (; ; ++i) {
			size_t map_sub_state = da[curr].m_map_sub_state;
			if (UINT32_MAX != map_sub_state) {
				size_t dfa_id = da[curr].m_subdfa_id;
				const  MatchingDFA* dfa = m_subdfa[dfa_id];
				const  BaseDAWG* dawg = dfa->get_dawg();
				size_t zidx = da[curr].m_zstr_idx;
				if (zidx) {
					assert(0 == ctx.zidx);
					assert(i == ctx.pos);
					ctx.zidx = zidx - 1;
				}
				ctx.pos = i;
				ctx.root = map_sub_state;
				i = match_dawg_sub(dawg, ctx, base_nth + idx, str, f, tr);
				ctx.root = make_id(dfa_id, map_sub_state, 0);
				return i;
			}
			assert(idx < n_words);
			assert(0 == ctx.zidx);
			if (da[curr].is_term())
				f(i, base_nth + idx++);
			if (str.size() == i)
				break;
			byte_t ch = (byte_t)tr((byte_t)str.p[i]);
			size_t child = da[curr].child0() + ch;
			if (da[child].parent() == curr) {
				abort(); // TODO: use double array for non-NLT DAWG
			//	idx += da[child].num;
				curr = child;
			} else
				break;
		}
		ctx.pos  = i;
		ctx.root = curr;
		return i;
	}
	size_t curr = ctx.root;
	size_t idx = 0;
	size_t i = ctx.pos;
	for (; ; ++i) {
		size_t curr_id = get_state_id(curr);
		if (curr >= max_base_state) {
			size_t dfa_id = get_subdfa_id(curr);
			const  MatchingDFA* dfa = m_subdfa[dfa_id];
			const  BaseDAWG* dawg = dfa->get_dawg();
			size_t zidx = get_zstr_idx(curr);
			if (zidx) {
				assert(0 == ctx.zidx);
				assert(i == ctx.pos);
				ctx.zidx = zidx - 1;
			}
			ctx.root = curr_id;
			i = match_dawg_sub(dawg, ctx, base_nth + idx, str, f, tr);
			ctx.root = make_id(dfa_id, ctx.root, 0);
			return i;
		}
		else {
			assert(idx < n_words);
			assert(0 == ctx.zidx);
			if (m_dyndfa.is_term(curr_id)) f(i, base_nth + idx++);
			if (str.size() == i) break;
			auto t = m_dyndfa.state_move(curr_id, (byte_t)tr(str[i]));
			if (nil_state == t.target) break;
			idx += t.num;
			curr = t.target;
		}
	}
	ctx.pos  = i;
	ctx.root = curr;
	return i;
}

size_t LazyUnionDAWG::v_state_to_word_id(size_t full_state) const {
	size_t subdfaId = get_subdfa_id(full_state);
	size_t substate = get_state_id(full_state);
	assert(subdfaId > 0);
	assert(subdfaId < m_subdfa.size());
	auto   dfa = m_subdfa[subdfaId];
	size_t base = m_word_num_vec[subdfaId - 1];
	return base + dfa->get_dawg()->v_state_to_word_id(substate);
}

size_t LazyUnionDAWG::state_to_dict_rank(size_t full_state) const {
	size_t subdfaId = get_subdfa_id(full_state);
	size_t substate = get_state_id(full_state);
	assert(subdfaId > 0);
	assert(subdfaId < m_subdfa.size());
	auto   dfa = m_subdfa[subdfaId];
	size_t base = m_word_num_vec[subdfaId - 1];
	return base + dfa->get_dawg()->state_to_dict_rank(substate);
}

size_t LazyUnionDAWG::index(MatchContext& ctx, fstring word)
const {
	if (m_has_suffix_cnt) {
		return index_dawg(ctx, word);
	}
	else if (m_da_trie.m_states.size()) {
		return index_nlt_da(ctx, word);
	}
	else {
		return index_nlt(ctx, word);
	}
}

void LazyUnionDAWG::lower_bound(MatchContext& ctx, fstring word, size_t* index, size_t* dict_rank) const {
    assert(index || dict_rank);
    if (m_has_suffix_cnt || m_da_trie.m_states.size() == 0) {
        TERARK_RT_assert(!m_has_suffix_cnt && m_da_trie.m_states.size() != 0,
            std::logic_error);
    }
    size_t curr = initial_state;
    auto da = m_da_trie.m_states.data();
    for (size_t i = 0; ; ++i) {
        size_t map_sub_state = da[curr].m_map_sub_state;
        if (terark_unlikely(UINT32_MAX != map_sub_state)) {
            size_t dfa_id = da[curr].m_subdfa_id;
            size_t state_zidx = da[curr].m_zstr_idx;
            const MatchingDFA* dfa = m_subdfa[dfa_id];
            assert(map_sub_state < dfa->v_total_states());
            if (state_zidx) {
                assert(0 == ctx.zidx);
                ctx.zidx = state_zidx - 1;
            }
            ctx.root = map_sub_state;
            ctx.pos = i;
            size_t idx, rank;
            dfa->get_dawg()->lower_bound(ctx, word, index ? &idx : nullptr, dict_rank ? &rank : nullptr);
            if (index) *index = m_word_num_vec[dfa_id - 1] + idx;
            if (dict_rank) *dict_rank = m_word_num_vec[dfa_id - 1] + rank;
            ctx.pos = word.size();
            ctx.root = make_id(dfa_id, ctx.root, 0);
            return;
        }
        assert(0 == ctx.zidx);
        assert(i <= word.size());
        if (terark_unlikely(word.size() == i)) {
            assert(0 == da[curr].m_zstr_idx);
            if (!da[curr].m_is_term)
                break;
            assert(curr < m_da_trie.m_term_submap.size());
            auto sub = m_da_trie.m_term_submap[curr];
            assert(sub.subdfa_id < m_subdfa.size());
            const MatchingDFA* dfa = m_subdfa[sub.subdfa_id];
            const BaseDAWG*  dawg = dfa->get_dawg();
            size_t base = m_word_num_vec[sub.subdfa_id - 1];
            if (index) *index = base + dawg->v_state_to_word_id(sub.substate_id);
            if (dict_rank) *dict_rank = base + dawg->state_to_dict_rank(sub.substate_id);
            ctx.root = make_id(0, curr, 0);
            return;
        }
        size_t t = da[curr].child0() + (byte_t)word.p[i];
        if (terark_unlikely(da[t].parent() != curr)) {
            break;
        }
        curr = t;
    }
    auto& bounds = m_dfa_str_bounds;
    size_t dfa_id = upper_bound_n(bounds, 1, bounds.size(), word);
    if (dfa_id < bounds.size()) {
        const MatchingDFA* dfa = m_subdfa[dfa_id];
        ctx.reset();
        size_t base = m_word_num_vec[dfa_id - 1];
        size_t idx, rank;
        dfa->get_dawg()->lower_bound(ctx, word, index ? &idx : nullptr, dict_rank ? &rank : nullptr);
        if (index) *index = base + idx;
        if (dict_rank) *dict_rank = base + rank;
        return;
    }
    if (index) *index = size_t(-1);
    if (dict_rank) *dict_rank = m_word_num_vec.back();
}

size_t LazyUnionDAWG::index_nlt(MatchContext& ctx, fstring word) const {
	size_t curr = initial_state;
	for(size_t i = 0; ; ++i) {
		size_t curr_id = get_state_id(curr);
		if (curr >= max_base_state) {
			size_t dfa_id = get_subdfa_id(curr);
			size_t state_zidx = get_zstr_idx(curr);
			const MatchingDFA* dfa = m_subdfa[dfa_id];
			if (state_zidx) {
				assert(0 == ctx.zidx);
				ctx.zidx = state_zidx - 1;
			}
			ctx.root = curr_id;
			ctx.pos = i;
			size_t idx2 = dfa->get_dawg()->index(ctx, word);
			if (null_word == idx2) {
				return null_word;
			}
			ctx.pos = word.size();
			ctx.root = make_id(dfa_id, ctx.root, 0);
			return m_word_num_vec[dfa_id-1] + idx2;
		}
		assert(0 == ctx.zidx);
		assert(i <= word.size());
		const auto& s = m_dyndfa.states[curr_id];
		if (word.size() == i) {
			if (s.is_term()) {
				assert(get_zstr_idx(curr) == 0);
				assert(curr_id < m_dyndfa.m_term_submap.size());
				auto sub = m_dyndfa.m_term_submap[curr_id];
				assert(sub.subdfa_id < m_subdfa.size());
				const MatchingDFA* dfa = m_subdfa[sub.subdfa_id];
				const BaseDAWG*  dawg = dfa->get_dawg();
				size_t base = m_word_num_vec[sub.subdfa_id-1];
				size_t idx = dawg->v_state_to_word_id(sub.substate_id);
				ctx.root = make_id(0, curr_id, 0);
				return base + idx;
			}
			return null_word;
		}
		auto t = m_dyndfa.state_move(s, (byte_t)word.p[i]);
		if (nil_state == t.target)
			return null_word;
		curr = t.target;
	}
	return size_t(-1); // depress compiler warning
}

size_t LazyUnionDAWG::index_nlt_da(MatchContext& ctx, fstring word) const {
	size_t curr = initial_state;
	auto da = m_da_trie.m_states.data();
	for(size_t i = 0; ; ++i) {
		size_t map_sub_state = da[curr].m_map_sub_state;
		if (terark_unlikely(UINT32_MAX != map_sub_state)) {
			size_t dfa_id = da[curr].m_subdfa_id;
			size_t state_zidx = da[curr].m_zstr_idx;
			const MatchingDFA* dfa = m_subdfa[dfa_id];
			assert(map_sub_state < dfa->v_total_states());
			if (state_zidx) {
				assert(0 == ctx.zidx);
				ctx.zidx = state_zidx - 1;
			}
			ctx.root = map_sub_state;
			ctx.pos = i;
			size_t idx2 = dfa->get_dawg()->index(ctx, word);
			if (null_word == idx2) {
				return null_word;
			}
			ctx.pos = word.size();
			ctx.root = make_id(dfa_id, ctx.root, 0);
			return m_word_num_vec[dfa_id-1] + idx2;
		}
		assert(0 == ctx.zidx);
		assert(i <= word.size());
		if (terark_unlikely(word.size() == i)) {
			if (da[curr].is_term()) {
				assert(0 == da[curr].m_zstr_idx);
				assert(curr < m_da_trie.m_term_submap.size());
				auto sub = m_da_trie.m_term_submap[curr];
				assert(sub.subdfa_id < m_subdfa.size());
				const MatchingDFA* dfa = m_subdfa[sub.subdfa_id];
				const BaseDAWG*  dawg = dfa->get_dawg();
				size_t base = m_word_num_vec[sub.subdfa_id-1];
				size_t idx = dawg->v_state_to_word_id(sub.substate_id);
				ctx.root = make_id(0, curr, 0);
				return base + idx;
			}
			return null_word;
		}
		size_t t = da[curr].child0() + (byte_t)word.p[i];
		if (da[t].parent() != curr)
			return null_word;
		curr = t;
	}
	return size_t(-1); // depress compiler warning
}

size_t LazyUnionDAWG::index_dawg(MatchContext& ctx, fstring word) const {
#if !defined(NDEBUG)
	size_t root_num = suffix_cnt(ctx.root);
#endif
	size_t curr = ctx.root;
	size_t idx = 0;
	for(size_t i = 0; ; ++i) {
		size_t curr_id = get_state_id(curr);
		if (curr >= max_base_state) {
			size_t dfa_id = get_subdfa_id(curr);
			size_t state_zidx = get_zstr_idx(curr);
			const MatchingDFA* dfa = m_subdfa[dfa_id];
			if (state_zidx) {
				assert(0 == ctx.zidx);
				ctx.zidx = state_zidx - 1;
			}
			ctx.root = curr_id;
			ctx.pos = i;
			size_t idx2 = dfa->get_dawg()->index(ctx, word);
			if (null_word == idx2) return null_word;
			ctx.root = make_id(dfa_id, ctx.root, 0);
			if (dfa->get_SuffixCountableDAWG()) {
				return idx + idx2;
			}
			return m_word_num_vec[dfa_id-1] + idx2;
		}
		assert(0 == ctx.zidx);
		assert(i <= word.size());
		const auto& s = m_dyndfa.states[curr_id];
		if (word.size() == i)
			return s.is_term() ? idx : null_word;
		auto t = m_dyndfa.state_move(s, (byte_t)word.p[i]);
		if (nil_state == t.target)
			return null_word;
		if (s.is_term()) {
			idx++;
			assert(idx < root_num);
		}
		idx += t.num;
		assert(idx < root_num);
		curr = t.target;
	}
	return size_t(-1); // depress compiler warning
}

DawgIndexIter
LazyUnionDAWG::dawg_lower_bound(MatchContext& ctx, fstring word) const {
#if !defined(NDEBUG)
	size_t root_num = m_has_suffix_cnt ? suffix_cnt(ctx.root) : 0;
#endif
	size_t curr = ctx.root;
	size_t idx = 0;
	for(size_t i = 0; ; ++i) {
		size_t curr_id = get_state_id(curr);
		if (curr >= max_base_state) {
			size_t dfa_id = get_subdfa_id(curr);
			size_t state_zidx = get_zstr_idx(curr);
			const MatchingDFA* dfa = m_subdfa[dfa_id];
			if (state_zidx) {
				assert(0 == ctx.zidx);
				ctx.zidx = state_zidx - 1;
			}
			ctx.root = curr_id;
			ctx.pos = i;
			DawgIndexIter dii2 = dfa->get_dawg()->dawg_lower_bound(ctx, word);
			ctx.root = make_id(dfa_id, ctx.root, 0);
			dii2.index += idx;
			return dii2;
		}
		assert(0 == ctx.zidx);
		assert(i <= word.size());
		const auto& s = m_dyndfa.states[curr_id];
		if (word.size() == i)
			return {idx, i, s.is_term()};
		auto t = m_dyndfa.state_move(s, (byte_t)word.p[i]);
		if (nil_state == t.target)
			return {idx, i, false};
		if (s.is_term()) {
			idx++;
			assert(idx < root_num);
		}
		idx += t.num;
		assert(idx < root_num);
		curr = t.target;
	}
	assert(0);
	return {size_t(-1), size_t(-1), false}; // depress compiler warning
}

void LazyUnionDAWG::nth_word(MatchContext& ctx, size_t nth, std::string* word)
const {
	if (!m_has_suffix_cnt) {
		if (initial_state != ctx.root || 0 != ctx.pos || 0 != ctx.zidx) {
			THROW_STD(invalid_argument,
				"ctx.root must be 0 on DAWG without suffix_cnt support");
		}
		size_t up = upper_bound_a(m_word_num_vec, nth);
		if (up < m_word_num_vec.size()) {
			assert(up > 0);
			size_t base_nth = m_word_num_vec[up-1];
			return m_subdfa[up]->get_dawg()->nth_word(ctx, nth - base_nth, word);
		}
		else {
			THROW_STD(out_of_range, "nth=%zd n_words=%zd", nth, n_words);
		}
	}

	assert(word->size() == ctx.pos);
#if !defined(NDEBUG)
	size_t root_num = suffix_cnt(ctx.root);
	assert(nth < root_num);
#endif
	size_t curr = ctx.root;
	size_t hit = 0;
	for (;;) {
		assert(hit <= nth);
		size_t curr_id = get_state_id(curr);
		if (curr >= max_base_state) {
			size_t zidx = get_zstr_idx(curr);
			size_t dfa_id = get_subdfa_id(curr);
			const MatchingDFA* dfa = m_subdfa[dfa_id];
			ctx.root = curr_id;
			ctx.zidx = zidx ? zidx - 1 : 0;
			ctx.pos = word->size();
		//	printf("ctx[root=%zd zidx=%zd pos=%zd] hit=%zd\n", ctx.root, ctx.zidx, ctx.pos, hit);
			dfa->get_dawg()->nth_word(ctx, nth-hit, word);
			return;
		}
		const auto& ss = m_dyndfa.states[curr_id];
		if (ss.is_term()) {
			if (hit == nth)
				break;
			++hit;
		}
		assert(hit <= nth);
		size_t next = nil_state;
		if (!ss.more_than_one_child()) {
			curr = m_dyndfa.single_target(ss);
		   	assert(nil_state != curr);
			word->push_back((byte_t)ss.getMinch());
			continue;
		}
		const int bits = ss.rlen();
		const auto& hb = m_dyndfa.pool.at<FastDynaDFA::header_max>(ss.getpos());
		const auto  base = hb.base(bits);
		const int cBlock = hb.align_bits(bits) / hb.BlockBits;
		byte_t the_ch = 0;
		byte_t min_ch = ss.getMinch();
		size_t inc = 0;
		for (int pos = 0, i = 0; i < cBlock; ++i) {
			FastDynaDFA::big_bm_b_t bm = hb.block(i);
			byte_t ch = min_ch + i * hb.BlockBits;
			for (; bm; ++pos) {
				const auto t = base[pos];
				int ctz = fast_ctz(bm);
				ch += ctz;
				if (hit + t.num <= nth)
					the_ch = ch, next = t.target, inc = t.num;
				else
					goto LoopEnd;
				ch++;
				bm >>= ctz; // must not be bm >>= ctz + 1
				bm >>= 1;   // because ctz + 1 may be bits of bm(32 or 64)
				assert(pos < hb.BlockBits * cBlock);
			}
		}
	LoopEnd:;
		assert(nil_state != next);
		word->push_back(the_ch);
		curr = next, hit += inc;
	}
}

size_t LazyUnionDAWG::
match_dawg(MatchContext& ctx, size_t base_nth, fstring str, const OnMatchDAWG& f)
const {
	return tpl_match_dawg(ctx, base_nth, str, f, IdentityTR());
}
size_t LazyUnionDAWG::
match_dawg(MatchContext& ctx, size_t base_nth, fstring str, const OnMatchDAWG& f, const ByteTR& tr)
const {
	return tpl_match_dawg<const ByteTR&>(ctx, base_nth, str, f, tr);
}
size_t LazyUnionDAWG::
match_dawg(MatchContext& ctx, size_t base_nth, fstring str, const OnMatchDAWG& f, const byte_t* tr)
const {
	assert(NULL != tr);
	return tpl_match_dawg(ctx, base_nth, str, f, TableTranslator(tr));
}

namespace LazyUnionDAWG_detail {
struct LongestOnMatch {
	size_t cnt = 0;
	size_t len = 0;
	size_t nth = 0;
	void operator()(size_t len2, size_t nth2) {
		cnt++;
		len = len2;
		nth = nth2;
	}
	bool get_result(size_t* p_len, size_t* p_nth) const {
		if (cnt) {
			*p_len = len;
			*p_nth = nth;
			return true;
		}
		return false;
	}
};
}
using namespace LazyUnionDAWG_detail;

bool LazyUnionDAWG::
match_dawg_l(MatchContext& ctx, fstring str, size_t* len, size_t* nth)
const {
	assert(NULL != len);
	assert(NULL != nth);
	LongestOnMatch on_match;
	tpl_match_dawg(ctx, 0, str, ref(on_match), IdentityTR());
	return on_match.get_result(len, nth);
}
bool LazyUnionDAWG::
match_dawg_l(MatchContext& ctx, fstring str, size_t* len, size_t* nth, const ByteTR& tr)
const {
	assert(NULL != len);
	assert(NULL != nth);
	LongestOnMatch on_match;
	tpl_match_dawg<const ByteTR&>(ctx, 0, str, ref(on_match), tr);
	return on_match.get_result(len, nth);
}
bool LazyUnionDAWG::
match_dawg_l(MatchContext& ctx, fstring str, size_t* len, size_t* nth, const byte_t* tr)
const {
	assert(NULL != len);
	assert(NULL != nth);
	assert(NULL != tr);
	LongestOnMatch on_match;
	tpl_match_dawg(ctx, 0, str, ref(on_match), TableTranslator(tr));
	return on_match.get_result(len, nth);
}

template<class Range>
struct VirtRange : ByteInputRange {
	Range& r;
	VirtRange(Range& r1) : r(r1) {}
	void operator++() override final { ++r; }
	byte_t operator*() override final { return *r; }
	bool empty() const override final { return r.empty(); }
};

/// string sets of DFAs should have no intersection, no overlap
/// but prefix can overlap, for example:
///  ppDFA[0]: ab, abc
///  ppDFA[1]: abcc, abcd  --- can not have "ab" or "abc"
///  ppDFA[2]: abcdd, ....
///  ...
BaseLazyUnionDFA*
createLazyUnionDFA(const MatchingDFA** ppDFA, size_t num, bool own) {
	std::unique_ptr<LazyUnionDFA> lazy_union;
	size_t num_dawgs = 0;
	for (size_t i = 0; i < num; ++i) {
		if (ppDFA[i]->get_dawg()) num_dawgs++;
	}
	if (num_dawgs == num)
		 lazy_union.reset(new LazyUnionDAWG);
	else lazy_union.reset(new LazyUnionDFA);
	for (size_t i = 0; i < num; ++i)
		lazy_union->add_dfa(ppDFA[i]);
	lazy_union->compile();
	lazy_union->set_own(own); // set own at last
	return lazy_union.release();
}

MatchingDFA*
loadAsLazyUnionDFA(fstring memory, bool ordered) {
	TERARK_RT_assert(memory.size() > 0, std::invalid_argument);
	valvec<std::unique_ptr<MatchingDFA>> dfa_vec;
	std::unique_ptr<MatchingDFA> dfa;
	size_t offset = 0;
	while (offset < memory.size()) {
		assert(size_t(memory.data() + offset) % 8 == 0);
		dfa.reset(MatchingDFA::load_mmap_user_mem(
			memory.data() + offset, memory.size() - offset));
		assert(dfa);
		offset += dfa->get_mmap().size();
		assert(offset <= memory.size());
		if (offset > memory.size()) {
			THROW_STD(out_of_range, "loadAsLazyUnionDFA bad memory");
		}
		dfa_vec.emplace_back(dfa.release());
	}
	if (dfa_vec.size() == 1) {
		return dfa_vec.front().release();
	}
	if (!ordered) {
		assert(false);
		THROW_STD(invalid_argument, "loadAsLazyUnionDFA must be ordered");
	}
	static_assert(sizeof(std::unique_ptr<MatchingDFA>) == sizeof(MatchingDFA*), "WTF ?");
	dfa.reset(createLazyUnionDFA(
		(const MatchingDFA**)dfa_vec.data(), dfa_vec.size(), true));
	assert(dfa);
	dfa_vec.risk_set_size(0);
	return dfa.release();
}
#else
	#pragma message("LazyUnionDFA is not supported on 32 bit platform")
#endif // TERARK_WORD_BITS >= 64

} // namespace terark
