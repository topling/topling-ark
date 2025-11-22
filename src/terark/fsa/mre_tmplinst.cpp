#include "tmplinst.hpp"
#if defined(TERARK_INST_ALL_DFA_TYPES)
#include "automata.hpp"
#endif
#include "dense_dfa.hpp"
#include "dense_dfa_v2.hpp"
#include "virtual_machine_dfa.hpp"
#include "mre_delim.hpp"
#include "mre_match.hpp"
#include "dfa_mmap_header.hpp"
#include <terark/util/autofree.hpp>
#include <terark/util/linebuf.hpp>
#include <terark/lcast.hpp>

BOOST_STATIC_ASSERT(TERARK_WORD_BITS == 32 || TERARK_WORD_BITS == 64);

namespace terark {

#pragma pack(push,1)
struct DynStateFlags {
#if TERARK_WORD_BITS == 32
	uint32_t  m_is_term : 1;
	uint32_t  reserved1 : sizeof(uint32_t)*8 - 1;
	uint32_t  cached_hash;
#else
	uint64_t  m_is_term   :  1;
	uint64_t  cached_hash : 63;
#endif
	uint32_t  ps_indx; // index to power_set
	uint32_t  ps_link; // hash link
	uint32_t  full_match_dest_state;

	DynStateFlags() {
		m_is_term = 0;
		cached_hash = 0;
	}

	void set_free_bit() { assert(0); }
	void set_pzip_bit() { assert(0); }
	void set_term_bit() { m_is_term = 1; }
	void clear_term_bit() { m_is_term = 0; }

	bool is_free() const { return false; }
	bool is_pzip() const { return false; }
	bool is_term() const { return m_is_term; }
};

struct DynDenseState : public DenseState<uint32_t, 256, DynStateFlags> {
	MyBitMap has_dnext;
	BOOST_STATIC_ASSERT(sizeof(size_t) == sizeof(bm_uint_t));

	DynDenseState() : has_dnext(1) {
		full_match_dest_state = nil_state; // unknown
	}
};
BOOST_STATIC_ASSERT(sizeof(DynDenseState) == 128);
#pragma pack(pop)

class DynDFA_Context;
typedef DenseDFA<uint32_t, 256, DynDenseState> DenseDFA_DynDFA_256_Base;
class DenseDFA_DynDFA_256 : public DenseDFA_DynDFA_256_Base {
	state_id_t* pBucket;
	size_t      nBucket;
	state_id_t  nStateNumComplete;
	state_id_t  nStateNumHotLimit;
	state_id_t  nPowerSetRoughLimit;

	size_t subset_hash(const state_id_t* beg, size_t len) const;
	bool subset_equal(const state_id_t* xbeg, size_t xlen, size_t y) const;

	///@param nfa is DynDFA_Context::dfa
	template<class DFA> void dyn_init_aux(const DFA* nfa);
	template<class DFA> void warm_up_aux(const DFA* nfa);
	void dyn_init_aux(const VirtualMachineDFA* nfa);
	void dyn_init_hash(size_t hsize);

	size_t add_next_set_impl(size_t oldsize, size_t dcurr, bool enableDiscard);

	template<class DFA>
	size_t add_next_set(const DFA* nfa, size_t oldsize, size_t dcurr, bool bEnableDiscard);
	size_t add_next_set(const VirtualMachineDFA*, size_t oldsize, size_t dcurr, bool enableDiscard);

	void hash_resize();

public:
	size_t maxmem;
	size_t refcnt;
	size_t dyn_root;
	size_t m_max_partial_match_len;
	bool m_show_dyn_stat;
	valvec<state_id_t>  power_set;

	DenseDFA_DynDFA_256();
	~DenseDFA_DynDFA_256();

	size_t dyn_mem_size() const;
	void dyn_discard_deep_cache();
	void dyn_init(const BaseDFA* nfa);
	void warm_up(const BaseDFA* nfa);

	template<class DFA>
	state_id_t dyn_state_move(const DFA* nfa, state_id_t dcurr, auchar_t ch);
	template<class DFA>
	state_id_t dyn_full_match_root(const DFA* nfa, state_id_t dcurr);

	void add_subset(size_t state, valvec<state_id_t>* subset) const;
	void get_subset(size_t state, valvec<state_id_t>* subset) const;

	template<class TR, class DFA>
	size_t
	full_match_with_tr(TR tr, const DFA* nfa,
		fstring text, valvec<int>* matched, DynDFA_Context*);

	template<class TR>
	size_t
	full_match_with_tr(TR tr, const VirtualMachineDFA* nfa,
		fstring text, valvec<int>* matched, DynDFA_Context*);

	template<class TR, class DFA>
	size_t
	full_match_all_with_tr_sort_uniq(TR tr, const DFA* nfa,
		fstring text, valvec<int>* matched, DynDFA_Context*);

	template<class TR>
	size_t
	full_match_all_with_tr_sort_uniq(TR tr, const VirtualMachineDFA* nfa,
		fstring text, valvec<int>* matched, DynDFA_Context*);

	template<class TR, class DFA>
	size_t
	full_match_all_with_tr_bitmap(TR tr, const DFA* nfa,
		fstring text, valvec<int>* matched, DynDFA_Context*);

	template<class TR>
	size_t
	full_match_all_with_tr_bitmap(TR tr, const VirtualMachineDFA* nfa,
		fstring text, valvec<int>* matched, DynDFA_Context*);

    template<class TR, class DFA>
	size_t
	shortest_full_match_with_tr(TR tr, const DFA* nfa,
		fstring text, valvec<int>* matched, DynDFA_Context*);

	template<class TR>
	size_t
	shortest_full_match_with_tr(TR tr, const VirtualMachineDFA* nfa,
		fstring text, valvec<int>* matched, DynDFA_Context*);

	template<class TR, class DFA>
	size_t
	full_match_all_len_with_tr(
			TR tr, const DFA* nfa, fstring text,
			valvec<MultiRegexFullMatch::LenRegexID>* matched,
			DynDFA_Context* ctx);

	template<class TR>
	size_t
	full_match_all_len_with_tr(
			TR tr, const VirtualMachineDFA* nfa, fstring text,
			valvec<MultiRegexFullMatch::LenRegexID>* matched,
			DynDFA_Context* ctx);
};

// instantiated these classes
#if TERARK_WORD_BITS >= 64 && defined(TERARK_INST_ALL_DFA_TYPES)
#define Automata_State32_512_ Automata<State32_512>
#else
// this makes it check DenseDFA_uint32_320 2 times, but does not make harm,
// just for pass compile
#define Automata_State32_512_ DenseDFA_uint32_320
#endif
TMPL_INST_DFA_CLASS(DenseDFA_uint32_320);
TMPL_INST_DFA_CLASS(DenseDFA_V2_uint32_288);
TMPL_INST_DFA_CLASS(VirtualMachineDFA);

template<class MatchClass>
struct MatchFuncPtr {
	typedef typename MatchClass::super super;
	terark_flatten
	static size_t
	match_func1(super* xThis, fstring text) {
		auto self = static_cast<MatchClass*>(xThis);
		return self->template match_with_tr<IdentityTR>(text, IdentityTR());
	}
	terark_flatten
	static size_t
	match_func2(super* xThis, fstring text, const ByteTR& tr) {
		auto self = static_cast<MatchClass*>(xThis);
		return self->template match_with_tr<const ByteTR&>(text, tr);
	}
	terark_flatten
	static size_t
	match_func3(super* xThis, fstring text, const byte_t* tr) {
		auto self = static_cast<MatchClass*>(xThis);
		return self->template match_with_tr<TableTranslator>(text, tr);
	}

	terark_flatten
	static size_t
	shortest_match_func1(super* xThis, fstring text) {
		auto self = static_cast<MatchClass*>(xThis);
		return self->template shortest_match_with_tr<IdentityTR>(text, IdentityTR());
	}
	terark_flatten
	static size_t
	shortest_match_func2(super* xThis, fstring text, const ByteTR& tr) {
		auto self = static_cast<MatchClass*>(xThis);
		return self->template shortest_match_with_tr<const ByteTR&>(text, tr);
	}
	terark_flatten
	static size_t
	shortest_match_func3(super* xThis, fstring text, const byte_t* tr) {
		auto self = static_cast<MatchClass*>(xThis);
		return self->template shortest_match_with_tr<TableTranslator>(text, tr);
	}
};

/// required when shorter match is requested
/// now this class is unused, because just return longest match
class DynDFA_Context {
public:
	struct MatchEntry {
		size_t curr_pos; // i
		size_t state_id;
	};
	valvec<MatchEntry> stack;
	valvec<uint32_t> matchid_states;
	febitvec hits;
};

#define DispatchConcreteDFA(MyClassName, OnDFA) \
	if (0) {} \
	OnDFA(MyClassName, Automata_State32_512_) \
	OnDFA(MyClassName, DenseDFA_uint32_320) \
	OnDFA(MyClassName, DenseDFA_V2_uint32_288) \
	OnDFA(MyClassName, VirtualMachineDFA) \
	else { \
		TERARK_THROW(std::invalid_argument \
			, "Don't support %s, dfa_class: %s" \
			, "MultiRegexFullMatchDynamicDfa", typeid(*dfa).name() \
			); \
	}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

template<class DFA>
inline static void
find_hits(const DFA* dfa, size_t matchState, febitvec* hits, valvec<int>* matchVec) {
	matchVec->erase_all();
	dfa_read_matchid(dfa, matchState, matchVec);
	assert(!matchVec->empty());
	for (size_t regexId : *matchVec) {
		hits->ensure_set1(regexId);
	}
}

inline static void collect_hits(febitvec* hits, valvec<int>* matchVec) {
	matchVec->erase_all();
	hits->push_back(1);
	hits->pop_back();
	for (size_t i = 0, n = hits->size(); i < n; ) {
		if (hits->is1(i)) {
			matchVec->push_back(int(i));
			i++;
		}
		else {
			i += hits->zero_seq_len(i);
		}
	}
}

struct MatchStateThreadLocal {
	gold_hash_set<int>           m_uniq_regex;
	gold_hash_set<int>           m_uniq_states;
	valvec<std::pair<int,int> >  m_pos_state_vec;
	void clean_reset() {
		m_uniq_regex.erase_all();
		m_uniq_states.erase_all();
		m_pos_state_vec.erase_all();
	}

	using LenRegexID = MultiRegexFullMatch::LenRegexID;
	valvec<LenRegexID>* m_cur_match = nullptr;
	int                 m_cur_len;

	///@{ for dfa_read_matchid
	terark_no_inline void push(int regex_id) { push_back(regex_id); }
	terark_no_inline void push_back(int regex_id) {
		if (m_uniq_regex.insert_i(regex_id).second)
			m_cur_match->push_back({m_cur_len, regex_id});
	}
	terark_no_inline void append(const int* src, size_t num) {
		LenRegexID* dst = m_cur_match->grow(num);
		for (size_t idx = 0; idx < num; idx++) {
			if (m_uniq_regex.insert_i(src[idx]).second)
				*dst++ = { m_cur_len, src[idx] };
		}
		m_cur_match->trim(dst);
	}
	size_t size() const { return m_cur_match->size(); } // for debug
	const LenRegexID& operator[](size_t i) const { return (*m_cur_match)[i]; }
	///@}

	template<class DFA>
	void collect(const DFA* au, valvec<LenRegexID>* cur_match) {
		m_cur_match = cur_match;
		while (!m_pos_state_vec.empty()) {
			auto [pos, state] = m_pos_state_vec.pop_val();
			if (m_uniq_states.insert_i(state).second) {
				m_cur_len = pos;
				dfa_read_matchid(au, state, this);
			}
		}
	}
};
static thread_local MatchStateThreadLocal g_MatchStateThreadLocal;

template<class DFA>
class MultiRegexFullMatchTmpl : public MultiRegexFullMatch {
public:
	febitvec m_hits;
	valvec<uint32_t> m_roots;
	valvec<int> m_idlist2;

	MultiRegexFullMatchTmpl(const DFA* au, bool isDyna) {
		if (isDyna) {
			DynamicDFA_get_roots(au, &m_roots);
		}
		else {
			m_roots.push_back(initial_state);
		}
		m_dfa = au;
	}

	template<class TR>
	size_t match_with_tr(fstring text, TR tr) {
		m_idlist2.erase_all();
		valvec<int>& idlist1 = m_regex_idvec;
		size_t maxlen = 0;
		for (size_t root : m_roots) {
			size_t curlen = match_with_tr_for(root, text, tr);
			if (curlen == maxlen) {
				m_idlist2.append(idlist1);
			}
			else if (curlen > maxlen) {
				maxlen = curlen;
				m_idlist2.swap(idlist1);
			}
		}
		idlist1.swap(m_idlist2);
		return maxlen;
	}
	template<class TR>
	size_t match_with_tr_for(size_t root, fstring text, TR tr) {
		const DFA* au = static_cast<const DFA*>(m_dfa);
		m_regex_idvec.erase_all();
		size_t curr = root;
		size_t last_state = 0;
		const byte_t* last_pos = NULL;
		const byte_t* pos = text.udata();
		const byte_t* end = pos + text.n;
		do {
			if (au->is_pzip(curr)) {
				fstring zs = au->get_zpath_data(curr, NULL);
				if (end - pos < zs.n)
					goto Done;
				for (intptr_t j = 0; j < zs.n; ++j, ++pos) {
					if (terark_unlikely((byte_t)tr(*pos) != zs[j]))
						goto Done;
				}
				size_t full = dfa_matchid_root(au, curr);
				if (terark_unlikely(DFA::nil_state != full))
					last_state = full, last_pos = pos;
				if (pos < end)
					curr = au->state_move(curr, (byte_t)tr(*pos++));
				else
					break;
			}
			else if (pos < end) {
				size_t next = dfa_loop_state_move<TR>(*au, curr, pos, end, tr);
				size_t full = dfa_matchid_root(au, curr);
				if (terark_unlikely(DFA::nil_state != full)) {
					last_state = full;
					last_pos = (curr == next) ? pos : pos-1;
				}
				curr = next;
			}
			else {
				size_t full = dfa_matchid_root(au, curr);
				if (terark_unlikely(DFA::nil_state != full))
					last_state = full, last_pos = pos;
				break;
			}
		} while (DFA::nil_state != curr);
	Done:
		if (last_pos) {
			dfa_read_matchid(au, last_state, &m_regex_idvec);
			assert(!this->empty());
			return last_pos - text.udata(); // fullmatch length
		}
		return 0; // fullmatch length
	}

	template<class TR>
	size_t shortest_match_with_tr(fstring text, TR tr) {
		m_idlist2.erase_all();
		valvec<int>& idlist1 = m_regex_idvec;
		size_t minlen = size_t(-1); // size_t max
		for (size_t root : m_roots) {
			size_t curlen = shortest_match_with_tr_for(root, text, tr);
			if (curlen == minlen) {
				m_idlist2.append(idlist1);
			}
			else if (curlen < minlen) {
				minlen = curlen;
				m_idlist2.swap(idlist1);
			}
		}
		idlist1.swap(m_idlist2);
		return minlen;
	}
	template<class TR>
	size_t shortest_match_with_tr_for(size_t root, fstring text, TR tr) {
		const DFA* au = static_cast<const DFA*>(m_dfa);
		m_regex_idvec.erase_all();
		size_t curr = root;
		size_t match_state = DFA::nil_state;
		const byte_t* pos = text.udata();
		const byte_t* end = pos + text.n;
		do {
			if (au->is_pzip(curr)) {
				fstring zs = au->get_zpath_data(curr, NULL);
				if (end - pos < zs.n)
					break;
				for (intptr_t j = 0; j < zs.n; ++j, ++pos) {
					if (terark_unlikely((byte_t)tr(*pos) != zs[j]))
						return 0;
				}
				match_state = dfa_matchid_root(au, curr);
				if (terark_unlikely(DFA::nil_state != match_state))
					goto Matched;
				if (pos < end)
					curr = au->state_move(curr, (byte_t)tr(*pos++));
				else
					break;
			}
			else {
				match_state = dfa_matchid_root(au, curr);
				if (terark_unlikely(DFA::nil_state != match_state))
					goto Matched;
				if (pos < end)
					curr = dfa_loop_state_move<TR>(*au, curr, pos, end, tr);
				else
					break;
			}
		} while (DFA::nil_state != curr);

		return 0;

	Matched:
		dfa_read_matchid(au, match_state, &m_regex_idvec);
		assert(!this->empty());
		return pos - text.udata(); // fullmatch length
	}

	template<class TR>
	size_t match_all_with_tr(fstring text, TR tr) {
		m_regex_idvec.erase_all();
		const DFA* au = static_cast<const DFA*>(m_dfa);
        if (au->m_atom_dfa_num < m_options->maxBitmapSize) {
            return match_all_with_tr_bitmap<TR>(text, tr);
        }
        else {
            return match_all_with_tr_sort_uniq<TR>(text, tr);
        }
    }

	template<class TR>
	size_t match_all_with_tr_sort_uniq(fstring text, TR tr) {
		for (size_t root : m_roots) {
			match_all_with_tr_sort_uniq_one<TR>(root, text, tr);
		}
		// two round unique
		m_regex_idvec.trim(std::unique(m_regex_idvec.begin(), m_regex_idvec.end()));
		std::sort(m_regex_idvec.begin(), m_regex_idvec.end());
		m_regex_idvec.trim(std::unique(m_regex_idvec.begin(), m_regex_idvec.end()));
		return this->size();
	}
	template<class TR>
	void match_all_with_tr_sort_uniq_one(size_t root, fstring text, TR tr) {
		const DFA* au = static_cast<const DFA*>(m_dfa);
		size_t curr = root;
		const byte_t* pos = text.udata();
		const byte_t* end = pos + text.n;
		do {
			if (au->is_pzip(curr)) {
				fstring zs = au->get_zpath_data(curr, NULL);
				if (end - pos < zs.n)
					return;
				for (intptr_t j = 0; j < zs.n; ++j, ++pos) {
					if (terark_unlikely((byte_t)tr(*pos) != zs[j]))
						return;
				}
				size_t full = dfa_matchid_root(au, curr);
				if (terark_unlikely(DFA::nil_state != full)) {
                    dfa_read_matchid(au, full, &m_regex_idvec);
				}
				if (pos < end)
					curr = au->state_move(curr, (byte_t)tr(*pos++));
				else
					break;
			}
			else if (pos < end) {
				size_t next = dfa_loop_state_move<TR>(*au, curr, pos, end, tr);
				size_t full = dfa_matchid_root(au, curr);
				if (terark_unlikely(DFA::nil_state != full)) {
                    dfa_read_matchid(au, full, &m_regex_idvec);
				}
				curr = next;
			}
			else {
				size_t full = dfa_matchid_root(au, curr);
				if (terark_unlikely(DFA::nil_state != full)) {
                    dfa_read_matchid(au, full, &m_regex_idvec);
				}
				break;
			}
		} while (DFA::nil_state != curr);
	}

    template<class TR>
	size_t match_all_with_tr_bitmap(fstring text, TR tr) {
		this->m_hits.erase_all();
		for (size_t root : m_roots) {
			match_all_with_tr_bitmap_one<TR>(root, text, tr);
		}
		collect_hits(&m_hits, &m_regex_idvec);
		return this->size();
	}
	template<class TR>
	void match_all_with_tr_bitmap_one(size_t root, fstring text, TR tr) {
		const DFA* au = static_cast<const DFA*>(m_dfa);
		size_t curr = root;
		const byte_t* pos = text.udata();
		const byte_t* end = pos + text.n;
		do {
			if (au->is_pzip(curr)) {
				fstring zs = au->get_zpath_data(curr, NULL);
				if (end - pos < zs.n)
					return;
				for (intptr_t j = 0; j < zs.n; ++j, ++pos) {
					if (terark_unlikely((byte_t)tr(*pos) != zs[j]))
						return;
				}
				size_t full = dfa_matchid_root(au, curr);
				if (terark_unlikely(DFA::nil_state != full)) {
					find_hits(au, full, &m_hits, &m_regex_idvec);
				}
				if (pos < end)
					curr = au->state_move(curr, (byte_t)tr(*pos++));
				else
					break;
			}
			else if (pos < end) {
				size_t next = dfa_loop_state_move<TR>(*au, curr, pos, end, tr);
				size_t full = dfa_matchid_root(au, curr);
				if (terark_unlikely(DFA::nil_state != full)) {
					find_hits(au, full, &m_hits, &m_regex_idvec);
				}
				curr = next;
			}
			else {
				size_t full = dfa_matchid_root(au, curr);
				if (terark_unlikely(DFA::nil_state != full)) {
					find_hits(au, full, &m_hits, &m_regex_idvec);
				}
				break;
			}
		} while (DFA::nil_state != curr);
	}

	template<class TR>
	void match_all_len_with_tr_one(size_t root, fstring text, TR tr) {
		const DFA* au = static_cast<const DFA*>(m_dfa);
		size_t curr = root;
		const byte_t* pos = text.udata();
		const byte_t* end = pos + text.n;
		auto& tls = g_MatchStateThreadLocal;
		tls.clean_reset();
		do {
			if (au->is_pzip(curr)) {
				fstring zs = au->get_zpath_data(curr, NULL);
				if (end - pos < zs.n)
					return;
				for (intptr_t j = 0; j < zs.n; ++j, ++pos) {
					if (terark_unlikely((byte_t)tr(*pos) != zs[j]))
						return;
				}
				size_t full = dfa_matchid_root(au, curr);
				if (terark_unlikely(DFA::nil_state != full)) {
					tls.m_pos_state_vec.push_back({int(pos - text.udata()), full});
				}
				if (pos < end)
					curr = au->state_move(curr, (byte_t)tr(*pos++));
				else
					break;
			}
			else if (pos < end) {
				size_t next = dfa_loop_state_move<TR>(*au, curr, pos, end, tr);
				size_t full = dfa_matchid_root(au, curr);
				if (terark_unlikely(DFA::nil_state != full)) {
					auto hitpos = (curr == next) ? pos : pos-1;
					tls.m_pos_state_vec.push_back({int(hitpos - text.udata()), full});
				}
				curr = next;
			}
			else {
				size_t full = dfa_matchid_root(au, curr);
				if (terark_unlikely(DFA::nil_state != full)) {
					tls.m_pos_state_vec.push_back({int(pos - text.udata()), full});
				}
				break;
			}
		} while (DFA::nil_state != curr);
		tls.collect(au, &m_cur_match);
	}
	template<class TR>
	size_t match_all_len_with_tr(fstring text, TR tr) {
		m_cur_match.erase_all();
		for (size_t root : m_roots) {
			match_all_len_with_tr_one<TR>(root, text, tr);
		}
		return m_cur_match.size();
	}

// terark_flatten here cause gcc hang, remove it
#undef terark_flatten
#define terark_flatten

	terark_flatten
	size_t match(fstring text) override {
		return match_with_tr(text, IdentityTR());
	}
	terark_flatten
	size_t match(fstring text, const ByteTR& tr) override {
		return match_with_tr<const ByteTR&>(text, tr);
	}
	terark_flatten
	size_t match(fstring text, const byte_t* tr) override {
		return match_with_tr<TableTranslator>(text, tr);
	}

	terark_flatten
	size_t shortest_match(fstring text) override {
		return shortest_match_with_tr(text, IdentityTR());
	}
	terark_flatten
	size_t shortest_match(fstring text, const ByteTR& tr) override {
		return shortest_match_with_tr<const ByteTR&>(text, tr);
	}
	terark_flatten
	size_t shortest_match(fstring text, const byte_t* tr) override {
		return shortest_match_with_tr<TableTranslator>(text, tr);
	}

	terark_flatten
	size_t match_all(fstring text) override {
		return match_all_with_tr(text, IdentityTR());
	}
	terark_flatten
	size_t match_all(fstring text, const ByteTR& tr) override {
		return match_all_with_tr<const ByteTR&>(text, tr);
	}
	terark_flatten
	size_t match_all(fstring text, const byte_t* tr) override {
		return match_all_with_tr<TableTranslator>(text, tr);
	}

	terark_flatten
	size_t match_all_len(fstring text) override {
		return match_all_len_with_tr(text, IdentityTR());
	}
	terark_flatten
	size_t match_all_len(fstring text, const ByteTR& tr) override {
		return match_all_len_with_tr<const ByteTR&>(text, tr);
	}
	terark_flatten
	size_t match_all_len(fstring text, const byte_t* tr) override {
		return match_all_len_with_tr<TableTranslator>(text, tr);
	}

	bool has_hit(int regex_id) const override {
		const DFA* au = static_cast<const DFA*>(m_dfa);
		if (au->m_atom_dfa_num < m_options->maxBitmapSize) {
			return size_t(regex_id) < m_hits.size() && m_hits.is1(regex_id);
		} else {
			return MultiRegexFullMatch::has_hit(regex_id);
		}
	}

	void clear_match_result() override {
		MultiRegexFullMatch::clear_match_result();
		m_hits.erase_all();
	}

	MultiRegexFullMatch* clone() const override {
		return new MultiRegexFullMatchTmpl(*this);
	}
};

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//

DenseDFA_DynDFA_256::DenseDFA_DynDFA_256() {
	m_max_partial_match_len = 0;
	refcnt = 1;
	maxmem = size_t(-1); // unlimited in initialize
	nStateNumComplete = 0;
	nStateNumHotLimit = 0;
	pBucket = NULL;
	nBucket = 0;
	m_show_dyn_stat = false;
	if (char* env = getenv("DenseDFA_DynDFA_256_show_dyn_stat")) {
		m_show_dyn_stat = (atoi(env) != 0);
	}
}
DenseDFA_DynDFA_256::~DenseDFA_DynDFA_256() {
	if (NULL != pBucket)
		::free(pBucket);
}

size_t DenseDFA_DynDFA_256::dyn_mem_size() const {
	return 0
		+ this->pool.size()
		+ sizeof(state_t) * states.size()
		+ sizeof(state_id_t) * power_set.size()
		+ sizeof(state_id_t) * nBucket
		;
}

inline
size_t
DenseDFA_DynDFA_256::subset_hash(const state_id_t* beg, size_t len)
const {
	assert(len > 0);
	size_t h = 0;
	size_t i = 0;
	do {
		h += beg[i];
		h *= 31;
		h += h << 3 | h >> (sizeof(h)*8-3);
	} while (++i < len);
#if TERARK_WORD_BITS == 32
	return h;
#elif TERARK_WORD_BITS == 64
	// just use 63 bits
	return h & (size_t(-1) >> 1);
#endif
}
inline
bool
DenseDFA_DynDFA_256::
subset_equal(const state_id_t* px0, size_t xlen, size_t y)
const {
	size_t iy0 = states[y+0].ps_indx;
	size_t iy1 = states[y+1].ps_indx;
	size_t ylen = iy1 - iy0;
	if (xlen != ylen) return false;
	assert(xlen > 0);
    assert(iy0 < power_set.size());
    assert(iy0 + ylen <= power_set.size());
	const state_id_t* py0 = power_set.data() + iy0;
	size_t i = 0;
	do {
		if (px0[i] != py0[i])
			return false;
	} while (++i < xlen);
	return true;
}

template<class StateID>
struct OneCharSubSet : valvec<StateID> {
	void resize0() {
		this->ch = -1;
		valvec<StateID>::resize0();
	}
	OneCharSubSet() { ch = -1; }
	ptrdiff_t  ch; // use ptrdiff_t for better align
};

void DenseDFA_DynDFA_256::dyn_discard_deep_cache() {
	if (m_show_dyn_stat) {
		size_t mem_cap = 0
			+ this->pool.capacity()
			+ sizeof(state_t) * states.capacity()
			+ sizeof(state_id_t) * power_set.capacity()
			+ sizeof(state_id_t) * nBucket
			;
		fprintf(stderr
			, "dyn_discard_deep_cache: power_set=%zd states=%zd"
			  " powerlen/states=%f mem_size=%zd mem_cap=%zd\n"
			, power_set.size(), states.size()
			, power_set.size()/(states.size()+0.0)
			, dyn_mem_size(), mem_cap
			);
	}
	assert(states.size() > nStateNumHotLimit+1);
	size_t my_sigma = this->get_sigma();
	AutoFree<CharTarget<size_t> > allmove(my_sigma);
	for(size_t dcurr = 0; dcurr < nStateNumHotLimit; ++dcurr) {
		if (states[dcurr].full_match_dest_state >= dyn_root + nStateNumHotLimit)
			states[dcurr].full_match_dest_state = nil_state; // set as unknown
	}
	for(size_t dcurr = nStateNumComplete; dcurr < nStateNumHotLimit; ++dcurr) {
		size_t nmove = get_all_move(dcurr, allmove.p);
		assert(nmove <= my_sigma);
		size_t i = 0;
		for(size_t j = 0; j < nmove; ++j) {
			if (allmove.p[j].target < dyn_root + nStateNumHotLimit)
				allmove.p[i++] = allmove.p[j];
		}
		if (i < nmove) {
			del_all_move(dcurr);
			add_all_move(dcurr, allmove.p, i);
		}
	}
	size_t nPowerSetHotLimit = states[nStateNumHotLimit].ps_indx;
	states.resize(nStateNumHotLimit);
	states.push_back(); // the guard
//	states.shrink_to_fit(); // don't shrink_to_fit?
	states.back().ps_indx = nPowerSetHotLimit;
	power_set.resize(nPowerSetHotLimit);
//	power_set.shrink_to_fit(); // don't shrink_to_fit?
	this->compact(); // required
	hash_resize();
}

void DenseDFA_DynDFA_256::dyn_init(const BaseDFA* dfa) {
	assert(power_set.size() == 0);
	dyn_root = dfa->v_total_states();
#define On_dyn_init(Class, DFA) \
	else if (const DFA* d = dynamic_cast<const DFA*>(dfa)) { \
		dyn_init_aux(d); }
	DispatchConcreteDFA(DenseDFA_DynDFA_256, On_dyn_init);
#undef On_dyn_init
	if (m_show_dyn_stat) {
		fprintf(stderr, "BaseDFA: roots=%zd states=%zd gnode_num=%zd\n"
				, power_set.size()
				, dfa->v_total_states()
				, dfa->v_gnode_states()
				);
	}
}

void DenseDFA_DynDFA_256::warm_up(const BaseDFA* dfa) {
	if (states.size() > 2) {
		fprintf(stderr
			, "DenseDFA_DynDFA_256 has been warmed up, power_set.size=%zd\n"
			, power_set.size());
		return;
	}
#define OnDFA(Class, DFA) \
	else if (const DFA* d = dynamic_cast<const DFA*>(dfa)) { \
		warm_up_aux(d); }
//	size_t oldsize = dyn->power_set.size();
	DispatchConcreteDFA(DenseDFA_DynDFA_256, OnDFA);
//	fprintf(stderr, "warm_up: power_set=%zd:%zd\n", oldsize, dyn->power_set.size());
#undef OnDFA
}

template<class DFA>
static void DynamicDFA_get_roots(const DFA* nfa, valvec<uint32_t>* roots) {
	roots->reserve(nfa->m_atom_dfa_num + nfa->m_dfa_cluster_num);
	auchar_t FakeEpsilonChar = 258;
	uint32_t curr = nfa->state_move(initial_state, FakeEpsilonChar);
	if (DFA::nil_state == curr) {
		THROW_STD(invalid_argument, "no transition on FakeEpsilonChar=258");
	}
	while (DFA::nil_state != curr) {
		roots->push_back(curr);
		curr = nfa->state_move(curr, FakeEpsilonChar);
	}
	assert(roots->size()+1 == nfa->m_atom_dfa_num + nfa->m_dfa_cluster_num);
	roots->erase_i(0, nfa->m_atom_dfa_num-1);
}

static void
DynamicDFA_get_roots(const VirtualMachineDFA* nfa, valvec<uint32_t>* roots) {
	assert(nfa->m_atom_dfa_num >= 1);
	roots->reserve(nfa->num_roots() - nfa->m_atom_dfa_num);
	for (size_t i = nfa->m_atom_dfa_num; i < nfa->num_roots(); ++i) {
		uint32_t curr = nfa->get_root(i);
		assert(nfa->nil_state != curr);
		roots->unchecked_push_back(curr);
	}
}

template<class DFA>
void DenseDFA_DynDFA_256::dyn_init_aux(const DFA* nfa) {
	power_set.reserve(nfa->total_states() * 2);
	DynamicDFA_get_roots(nfa, &power_set);
	dyn_init_hash(nfa->total_states() * 1 / 4);
	size_t dnext = add_next_set_impl(0, 0, false);
	assert(dyn_root == dnext); TERARK_UNUSED_VAR(dnext);
	assert(states.size() == 2);
}

void DenseDFA_DynDFA_256::dyn_init_aux(const VirtualMachineDFA* nfa) {
	power_set.reserve(nfa->v_gnode_states() * 2);
	DynamicDFA_get_roots(nfa, &power_set);
	dyn_init_hash(nfa->v_gnode_states() * 1 / 16);
	size_t dnext = add_next_set(nfa, 0, 0, false);
	assert(dyn_root == dnext); TERARK_UNUSED_VAR(dnext);
	assert(states.size() == 2);
}

void DenseDFA_DynDFA_256::dyn_init_hash(size_t hsize) {
	nPowerSetRoughLimit = hsize;
	states.reserve(hsize); // hsize is not nBucket
	states.resize(1);
	states[0].ps_indx = 0;
	nBucket = __hsm_stl_next_prime(hsize * 3/2);
	pBucket = (state_id_t*)malloc(sizeof(state_id_t) * nBucket);
	if (NULL == pBucket) {
		THROW_STD(runtime_error, "malloc(%zd) hash bucket failed",
				sizeof(state_id_t) * nBucket);
	}
	if (m_show_dyn_stat) {
		fprintf(stderr, "dyn_init_hash: hsize=%zd nBucket=%zd\n"
			, hsize, nBucket);
	}
	std::fill_n(pBucket, nBucket, nil_state+0);
}

template<class DFA>
void DenseDFA_DynDFA_256::warm_up_aux(const DFA* nfa) {
	if (m_show_dyn_stat) {
		fprintf(stderr, "warm_up: nBucket=%zd states.capacity=%zd power_set.size=%zd\n"
			, nBucket, states.capacity(), power_set.size());
	}

// discover DynDFA states near initial_state...
//
	size_t src_sigma = nfa->get_sigma();
	smallmap<OneCharSubSet<state_id_t> > chmap(src_sigma);
	terark::AutoFree<CharTarget<size_t> > allmove(src_sigma);
	// this loop is an implicit BFS search
	// states.size() is increasing during the loop execution
	for(size_t dcurr = 0; dcurr < states.size()-1; ++dcurr) {
		size_t sub_beg = states[dcurr+0].ps_indx;
		size_t sub_end = states[dcurr+1].ps_indx;
		chmap.resize0();
		for(size_t j = sub_beg; j < sub_end; ++j) {
			size_t snfa = power_set[j];
			size_t nmove = nfa->get_all_move(snfa, allmove.p);
			TERARK_VERIFY_LE(nmove, src_sigma);
			for(size_t k = 0; k < nmove; ++k) {
				auto  ct = allmove.p[k];
				static_assert(FULL_MATCH_DELIM == 257);
				if (ct.ch < 256 || ct.ch == FULL_MATCH_DELIM)
					chmap.bykey(ct.ch).push_back(ct.target);
			}
		}
		states[dcurr].has_dnext.fill(0);
		size_t i = 0;
		for(size_t j = 0; j < chmap.size(); ++j) {
			auto& subset = chmap.byidx(j);
			size_t oldsize = power_set.size();
			power_set.append(subset);
			state_id_t dnext = add_next_set(nfa, oldsize, dcurr, false);
			if (nil_state == dnext) {
				THROW_STD(runtime_error, "out of memory on warming up");
			}
			if (subset.ch >= 256) {
				if (subset.ch == FULL_MATCH_DELIM) {
					states[dcurr].full_match_dest_state = dnext;
				}
				continue; // don't i++
			}
			states[dcurr].has_dnext.set1(subset.ch);
			allmove.p[i].ch = subset.ch;
			allmove.p[i].target = dnext;
			i++;
		}
		std::sort(allmove.p, allmove.p + i, CharTarget_By_ch());
		this->add_all_move(dcurr, allmove.p, i);
		nStateNumComplete = dcurr+1;
		if (power_set.size() >= nPowerSetRoughLimit)
			break;
	}
	nStateNumHotLimit = states.size()-1; // exact limit
	size_t minmem = 8*1024 // plus all capacity sum
		+ this->pool.capacity()
		+ sizeof(state_t) * states.capacity()
		+ sizeof(state_id_t) * power_set.capacity()
		+ sizeof(state_id_t) * nBucket
		;
	if (size_t(-1) == maxmem)
		maxmem = 2 * minmem;
	else if (maxmem < minmem) {
		fprintf(stderr
			, "WARN:%s:%d: func=%s : maxmem=%zd < minmem=%zd, use default\n"
			, __FILE__, __LINE__, BOOST_CURRENT_FUNCTION
			, maxmem, minmem);
		maxmem = minmem;
	}

	if (m_show_dyn_stat) {
		fprintf(stderr
			, "warmed: Complete=%u HotLimit=%u"
			 " Bucket=%zd Power=%zd"
			 " Power/Complete=%f"
			 " Power/HotLimit=%f"
			 "\n"
			, nStateNumComplete, nStateNumHotLimit
			, nBucket, power_set.size()
			, power_set.size()/(nStateNumComplete+0.0)
			, power_set.size()/(nStateNumHotLimit+0.0)
			);
	}
}

// single thread
template<class DFA> // typename
DenseDFA_DynDFA_256::state_id_t
DenseDFA_DynDFA_256::
dyn_state_move(const DFA* nfa, state_id_t dcurr, auchar_t ch) {
	if (dcurr < dyn_root) {
		return nfa->state_move(dcurr, ch);
	}
	dcurr -= dyn_root;
	if (states[dcurr].has_dnext.is0(ch)) {
		return nil_state;
	}
	state_id_t dnext = this->state_move(dcurr, ch);
	if (nil_state != dnext)
		return dnext;

	size_t oldsize = this->power_set.size();
	size_t sub_beg = this->states[dcurr + 0].ps_indx;
	size_t sub_end = this->states[dcurr + 1].ps_indx;
	assert(sub_beg < sub_end);
	this->power_set.resize_no_init(power_set.size() + sub_end - sub_beg);
	state_id_t* power_data = power_set.begin();
	size_t newsize = oldsize;
	for (size_t j = sub_beg; j < sub_end; ++j) {
		state_id_t curr = power_set[j];
		state_id_t next = nfa->state_move(curr, ch);
		if (DFA::nil_state != next)
			power_data[newsize++] = next;
	}
	power_set.risk_set_size(newsize);
	if (oldsize == newsize) {
		states[dcurr].has_dnext.set0(ch);
	}
	else {
		dnext = add_next_set(nfa, oldsize, dcurr, true);
		add_move_checked(dcurr, dnext, ch);
	}
	return dnext;
}

template<class DFA>
inline // typename
DenseDFA_DynDFA_256::state_id_t
DenseDFA_DynDFA_256::
dyn_full_match_root(const DFA* nfa, state_id_t dcurr) {
	BOOST_STATIC_ASSERT(FULL_MATCH_DELIM < DFA::sigma);
	assert(FULL_MATCH_DELIM < nfa->get_sigma());
	if (dcurr < dyn_root) {
		return nfa->state_move(dcurr, FULL_MATCH_DELIM);
	}
	dcurr -= dyn_root;
	state_id_t dnext = states[dcurr].full_match_dest_state;
	const uint32_t NOT_A_FULL_MATCH = nil_state - 1;
	if (NOT_A_FULL_MATCH == dnext)
		return nil_state;
	if (dnext < NOT_A_FULL_MATCH)
		return dnext;
	assert(nil_state == dnext); // unknown, to be determined

// begin copy from dyn_state_move
	size_t oldsize = this->power_set.size();
	size_t sub_beg = this->states[dcurr + 0].ps_indx;
	size_t sub_end = this->states[dcurr + 1].ps_indx;
	assert(sub_beg < sub_end);
	this->power_set.resize_no_init(power_set.size() + sub_end - sub_beg);
	state_id_t* power_data = power_set.begin();
	size_t newsize = oldsize;
	for (size_t j = sub_beg; j < sub_end; ++j) {
		state_id_t curr = power_set[j];
		state_id_t next = nfa->state_move(curr, FULL_MATCH_DELIM);
		if (DFA::nil_state != next)
			power_data[newsize++] = next;
	}
	power_set.risk_set_size(newsize);
// end copy from dyn_state_move

	if (oldsize == newsize) {
	   	// now, we known there is not a full match
		states[dcurr].full_match_dest_state = NOT_A_FULL_MATCH;
	} else {
		// add new dnext, it is a full match
		dnext = add_next_set(nfa, oldsize, dcurr, true);
		states[dcurr].full_match_dest_state = dnext;
	}
	return dnext;
}

// This function should always success
// cache discarding will be delayed
size_t
DenseDFA_DynDFA_256::add_next_set_impl
(size_t oldsize, size_t dcurr, bool enableDiscard)
{
	// this is a specialized hash table find or insert
	assert(states.back().ps_indx == oldsize);
	state_id_t* next_beg = power_set.begin() + oldsize;
	  std::sort(next_beg , power_set.end());
	state_id_t* next_end = std::unique(next_beg, power_set.end());
	size_t next_len = next_end - next_beg;
	if (1 == next_len) {
		// the unique state in subset
		power_set.risk_set_size(oldsize);
		return next_beg[0];
	}
	size_t hash = subset_hash(next_beg, next_len);
	size_t ihit = hash % nBucket;
	state_id_t dstate = pBucket[ihit];
	while (nil_state != dstate) {
		if (subset_equal(next_beg, next_len, dstate)) {
			// the subset dstate has existed, return it
			power_set.risk_set_size(oldsize);
			return state_id_t(dyn_root + dstate);
		}
		dstate = states[dstate].ps_link;
	}
	// the subset is not existed, add it
	power_set.trim(next_end);
	if (enableDiscard && dcurr < nStateNumHotLimit && dyn_mem_size() > maxmem) {
		valvec<state_id_t> tmp(next_beg, next_end);
		dyn_discard_deep_cache();
		power_set.append(tmp);
        //
        // Note:
        //   although dyn_discard_deep_cache() is called at many places
        //   it still has pitfalls, for example in a large text match and
        //   dcurr is always >= nStateNumHotLimit, there is no chance to
        //   call dyn_discard_deep_cache(), and the memory limit will be
        //   exceedded. Fortunately, this case is very rare, and the memory
        //   limit is just a soft limit...
        //   It needs many code changes to throughly fix this very rare
        //   issue, so just let it be...
	}
	size_t dnext = states.size()-1;
	bool should_realloc = states.size() == states.capacity();
	states.push_back();
	states[dnext+0].cached_hash = hash;
	states[dnext+1].ps_indx = power_set.size();
	if (!should_realloc) {
	   	// insert to hash table
		states[dnext].ps_link = pBucket[ihit];
		pBucket[ihit] = state_id_t(dnext);
	}
   	else {
		hash_resize();
	//	fprintf(stderr, "hash_resize in add_next_set, states=%zd nBucket=%u\n", states.size(), nBucket);
	}
	return dyn_root + dnext;
}

template<class DFA>
size_t
DenseDFA_DynDFA_256::add_next_set
(const DFA*, size_t oldsize, size_t dcurr, bool enableDiscard)
{
	assert(power_set.size() > oldsize);
	return add_next_set_impl(oldsize, dcurr, enableDiscard);
}

size_t
DenseDFA_DynDFA_256::add_next_set
(const VirtualMachineDFA* nfa, size_t oldsize, size_t dcurr, bool enableDiscard)
{
	assert(power_set.size() > oldsize);
	size_t dnext = add_next_set_impl(oldsize, dcurr, enableDiscard);
	size_t newsize = power_set.size();
	if (newsize > oldsize) {
		assert(dnext >= dyn_root);
		assert(newsize - oldsize >= 2);
		for(size_t j = oldsize; j < newsize; ++j) {
			size_t s = power_set[j];
			if (nfa->is_term(s)) {
				this->set_term_bit(dnext - dyn_root);
				break;
			}
		}
	}
	return dnext;
}

void DenseDFA_DynDFA_256::hash_resize() {
	assert(NULL != pBucket);
	size_t uBucket = __hsm_stl_next_prime(states.capacity() * 3/2);
	if (nBucket != uBucket) {
		size_t bucket_mem_size = sizeof(state_id_t) * uBucket;
		::free(pBucket);
		nBucket = uBucket;
		pBucket = (state_id_t*)malloc(bucket_mem_size);
		if (NULL == pBucket) {
			THROW_STD(runtime_error, "malloc(%zd)", bucket_mem_size);
		}
	}
	std::fill_n(pBucket, uBucket, nil_state+0);
#ifndef NDEBUG
	const state_id_t* power_data = power_set.data();
#endif
	auto pstates = &states[0];
	auto qBucket = pBucket; // compiler will optimize local var
	size_t n = states.size() - 1;
	for(size_t i = 0; i < n; ++i) {
		size_t hash = pstates[i].cached_hash;
		size_t ihit = hash % uBucket;
	#ifndef NDEBUG
		size_t ibeg = pstates[i+0].ps_indx;
		size_t iend = pstates[i+1].ps_indx;
		size_t ilen = iend - ibeg;
		assert(ilen > 1);
		assert(iend <= power_set.size());
		size_t computed_hash = subset_hash(power_data + ibeg, ilen);
		assert(computed_hash == hash);
	#endif
		pstates[i].ps_link = qBucket[ihit];
		qBucket[ihit] = state_id_t(i);
	}
}

void
DenseDFA_DynDFA_256::get_subset(size_t state, valvec<state_id_t>* subset)
const {
	subset->erase_all();
	add_subset(state, subset);
}
void
DenseDFA_DynDFA_256::add_subset(size_t state, valvec<state_id_t>* subset)
const {
	if (state < dyn_root)
		subset->push(state);
	else {
		state -= dyn_root;
		assert(state + 1 < states.size());
		size_t sub_beg = states[state + 0].ps_indx;
		size_t sub_end = states[state + 1].ps_indx;
		assert(sub_end - sub_beg >= 2);
		for (size_t k = sub_beg; k < sub_end; ++k)
			subset->push(power_set[k]);
	}
}

// single-thread matching is faster in all tested cases
template<class TR, class DFA>
size_t
DenseDFA_DynDFA_256::full_match_with_tr(TR tr, const DFA* nfa,
		fstring text, valvec<int>* matched, DynDFA_Context* ctx)
{
	matched->erase_all();
	assert(power_set.size() >= 2);
	size_t curr = dyn_root;
	const byte_t* last_pos = NULL;
	const byte_t* pos = text.udata();
	const byte_t* end = pos + text.size();
	do {
		size_t full = dyn_full_match_root(nfa, curr);
		if (nil_state != full) {
			get_subset(full, &ctx->matchid_states);
		}
		if (pos < end) {
			size_t next;
			do next = dyn_state_move(nfa, curr, (byte_t)tr(*pos));
			while (++pos < end && next == curr);
			if (nil_state != full)
			   	last_pos = (curr == next) ? pos : pos-1;
			curr = next;
		}
	   	else {
			assert(pos == end);
			if (nil_state != full) last_pos = end;
			break;
		}
	} while (nil_state != curr);

	if (terark_unlikely(dyn_mem_size() > maxmem)) {
		dyn_discard_deep_cache();
	}
	m_max_partial_match_len = pos - text.udata();
	if (last_pos) {
		assert(ctx->matchid_states.size());
		for (size_t i = 0; i < ctx->matchid_states.size(); ++i)
			dfa_read_matchid(nfa, ctx->matchid_states[i], matched);
		assert(!matched->empty());
		std::sort(matched->begin(), matched->end());
		return last_pos - text.udata();
	}
	return 0;
}

template<class TR>
size_t
DenseDFA_DynDFA_256::full_match_with_tr(TR tr, const VirtualMachineDFA* nfa,
	   	fstring text, valvec<int>* matched, DynDFA_Context* ctx)
{
	matched->erase_all();
	assert(power_set.size() >= 2);
	size_t curr = dyn_root;
	const byte_t* last_pos = NULL;
	const byte_t* pos = text.udata();
	const byte_t* end = pos + text.size();
	BOOST_STATIC_ASSERT(VirtualMachineDFA::nil_state == nil_state);
	do {
		if (curr < dyn_root) {
			if (pos < end) {
				size_t next = nfa->loop_state_move<TR>(curr, pos, end, tr);
				if (nfa->is_term(curr)) {
					ctx->matchid_states.resize_fill(1, curr);
					last_pos = (curr == next) ? pos : pos-1;
				}
				curr = next;
			}
			else {
				assert(pos == end);
				if (nfa->is_term(curr)) {
					ctx->matchid_states.resize_fill(1, curr);
					last_pos = pos;
				}
				break;
			}
		}
		else {
			if (pos < end) {
				size_t next;
				do next = dyn_state_move(nfa, curr, (byte_t)tr(*pos));
				while (++pos < end && next == curr);
				if (this->is_term(curr - dyn_root)) {
					get_subset(curr, &ctx->matchid_states);
					last_pos = (curr == next) ? pos : pos-1;
				}
				curr = next;
			}
			else {
				assert(pos == end);
				if (this->is_term(curr - dyn_root)) {
					get_subset(curr, &ctx->matchid_states);
					last_pos = pos;
				}
				break;
			}
		}
	} while (nil_state != curr);

	if (terark_unlikely(dyn_mem_size() > maxmem)) {
		dyn_discard_deep_cache();
	}
	m_max_partial_match_len = pos - text.udata();
	if (last_pos) {
		assert(ctx->matchid_states.size());
		for(size_t i = 0; i < ctx->matchid_states.size(); ++i) {
			size_t s = ctx->matchid_states[i];
			if (nfa->is_term(s))
				nfa->read_matchid(s, matched);
		}
		assert(!matched->empty());
		std::sort(matched->begin(), matched->end());
		return last_pos - text.udata();
	}
	return 0;
}

template<class TR, class DFA>
size_t
DenseDFA_DynDFA_256::shortest_full_match_with_tr(TR tr, const DFA* nfa,
		fstring text, valvec<int>* matched, DynDFA_Context* ctx)
{
	matched->erase_all();
	assert(power_set.size() >= 2);

	if (terark_unlikely(dyn_mem_size() > maxmem)) {
		dyn_discard_deep_cache();
	}
	size_t curr = dyn_root;
	const byte_t* pos = text.udata();
	const byte_t* end = pos + text.size();
	do {
		assert(pos <= end);
		size_t full = dyn_full_match_root(nfa, curr);
		if (nil_state != full) {
			get_subset(full, &ctx->matchid_states);
			goto Matched;
		}
		if (pos < end) {
			size_t next;
			do next = dyn_state_move(nfa, curr, (byte_t)tr(*pos));
			while (++pos < end && next == curr);
			curr = next;
		} else
			break;
	} while (nil_state != curr);

	m_max_partial_match_len = pos - text.udata();
	return 0;

Matched:
	m_max_partial_match_len = pos - text.udata();
	assert(ctx->matchid_states.size());
	for (size_t i = 0; i < ctx->matchid_states.size(); ++i)
		dfa_read_matchid(nfa, ctx->matchid_states[i], matched);
	assert(!matched->empty());
	std::sort(matched->begin(), matched->end());
	return pos - text.udata();
}

template<class TR>
size_t
DenseDFA_DynDFA_256::shortest_full_match_with_tr(TR tr, const VirtualMachineDFA* nfa,
		fstring text, valvec<int>* matched, DynDFA_Context* ctx)
{
	matched->erase_all();
	assert(power_set.size() >= 2);

	if (terark_unlikely(dyn_mem_size() > maxmem)) {
		dyn_discard_deep_cache();
	}
	size_t curr = dyn_root;
	const byte_t* pos = text.udata();
	const byte_t* end = pos + text.size();
	BOOST_STATIC_ASSERT(VirtualMachineDFA::nil_state == nil_state);
	do {
		assert(pos <= end);
		if (curr < dyn_root) {
			if (nfa->is_term(curr)) {
				ctx->matchid_states.resize_fill(1, curr);
				goto Matched;
			}
			if (pos < end)
				curr = nfa->loop_state_move<TR>(curr, pos, end, tr);
			else
				break;
		}
		else {
			if (this->is_term(curr - dyn_root)) {
				get_subset(curr, &ctx->matchid_states);
				goto Matched;
			}
			if (pos < end) {
				size_t next;
				do next = dyn_state_move(nfa, curr, (byte_t)tr(*pos));
				while (++pos < end && next == curr);
				curr = next;
			} else
				break;
		}
	} while (nil_state != curr);

	m_max_partial_match_len = pos - text.udata();
	return 0;

Matched:
	m_max_partial_match_len = pos - text.udata();
	assert(ctx->matchid_states.size());
	for(size_t i = 0; i < ctx->matchid_states.size(); ++i) {
		size_t s = ctx->matchid_states[i];
		if (nfa->is_term(s))
			nfa->read_matchid(s, matched);
	}
	assert(!matched->empty());
	std::sort(matched->begin(), matched->end());
	return pos - text.udata();
}

template<class TR, class DFA>
size_t
DenseDFA_DynDFA_256::full_match_all_with_tr_sort_uniq(
        TR tr, const DFA* nfa,
	   	fstring text, valvec<int>* matched, DynDFA_Context* ctx)
{
	matched->erase_all();
	ctx->hits.resize_fill(0, false);
	assert(power_set.size() >= 2);
	size_t curr = dyn_root;
	const byte_t* pos = text.udata();
	const byte_t* end = pos + text.size();
	do {
		size_t full = dyn_full_match_root(nfa, curr);
		if (nil_state != full) {
			get_subset(full, &ctx->matchid_states);
			for (size_t matchState: ctx->matchid_states)
				dfa_read_matchid(nfa, matchState, matched);
		}
		if (pos < end) {
			size_t next;
			do next = dyn_state_move(nfa, curr, (byte_t)tr(*pos));
			while (++pos < end && next == curr);
			curr = next;
		}
	   	else {
			assert(pos == end);
			break;
		}
	} while (nil_state != curr);
	m_max_partial_match_len = pos - text.udata();

	if (terark_unlikely(dyn_mem_size() > maxmem)) {
		dyn_discard_deep_cache();
	}
    // two round unique
    matched->trim(std::unique(matched->begin(), matched->end()));
    std::sort(matched->begin(), matched->end());
    matched->trim(std::unique(matched->begin(), matched->end()));
	return matched->size();
}

template<class TR>
size_t
DenseDFA_DynDFA_256::full_match_all_with_tr_sort_uniq(
        TR tr, const VirtualMachineDFA* nfa,
		fstring text, valvec<int>* matched, DynDFA_Context* ctx)
{
	matched->erase_all();
	ctx->hits.resize_fill(0, false);
	assert(power_set.size() >= 2);
	size_t curr = dyn_root;
	const byte_t* pos = text.udata();
	const byte_t* end = pos + text.size();
	BOOST_STATIC_ASSERT(VirtualMachineDFA::nil_state == nil_state);
	do {
		if (curr < dyn_root) {
			if (pos < end) {
				size_t next = nfa->loop_state_move<TR>(curr, pos, end, tr);
				if (nfa->is_term(curr)) {
					dfa_read_matchid(nfa, curr, matched);
				}
				curr = next;
			}
			else {
				assert(pos == end);
				if (nfa->is_term(curr)) {
				    dfa_read_matchid(nfa, curr, matched);
				}
				break;
			}
		}
		else {
			auto try_hit_curr = [&]() {
				if (this->is_term(curr - dyn_root)) {
					get_subset(curr, &ctx->matchid_states);
					for (size_t matchState : ctx->matchid_states)
						dfa_read_matchid(nfa, matchState, matched);
				}
			};
			if (pos < end) {
				size_t next;
				do next = dyn_state_move(nfa, curr, (byte_t)tr(*pos));
				while (++pos < end && next == curr);
				try_hit_curr();
				curr = next;
			}
			else {
				assert(pos == end);
				try_hit_curr();
				break;
			}
		}
	} while (nil_state != curr);

	if (terark_unlikely(dyn_mem_size() > maxmem)) {
		dyn_discard_deep_cache();
	}
	m_max_partial_match_len = pos - text.udata();
    // two round unique
    matched->trim(std::unique(matched->begin(), matched->end()));
    std::sort(matched->begin(), matched->end());
    matched->trim(std::unique(matched->begin(), matched->end()));
	return matched->size();
}

template<class TR, class DFA>
size_t
DenseDFA_DynDFA_256::full_match_all_with_tr_bitmap(TR tr, const DFA* nfa,
		fstring text, valvec<int>* matched, DynDFA_Context* ctx)
{
	matched->erase_all();
	ctx->hits.resize_fill(0, false);
	assert(power_set.size() >= 2);
	size_t curr = dyn_root;
	const byte_t* pos = text.udata();
	const byte_t* end = pos + text.size();
	do {
		size_t full = dyn_full_match_root(nfa, curr);
		if (nil_state != full) {
			get_subset(full, &ctx->matchid_states);
			for (size_t matchState: ctx->matchid_states)
				find_hits(nfa, matchState, &ctx->hits, matched);
		}
		if (pos < end) {
			size_t next;
			do next = dyn_state_move(nfa, curr, (byte_t)tr(*pos));
			while (++pos < end && next == curr);
			curr = next;
		}
		else {
			assert(pos == end);
			break;
		}
	} while (nil_state != curr);

	if (terark_unlikely(dyn_mem_size() > maxmem)) {
		dyn_discard_deep_cache();
	}
	m_max_partial_match_len = pos - text.udata();
	collect_hits(&ctx->hits, matched);
	return matched->size();
}

template<class TR>
size_t
DenseDFA_DynDFA_256::full_match_all_with_tr_bitmap(TR tr, const VirtualMachineDFA* nfa,
		fstring text, valvec<int>* matched, DynDFA_Context* ctx)
{
	matched->erase_all();
	ctx->hits.resize_fill(0, false);
	assert(power_set.size() >= 2);
	size_t curr = dyn_root;
	const byte_t* pos = text.udata();
	const byte_t* end = pos + text.size();
	BOOST_STATIC_ASSERT(VirtualMachineDFA::nil_state == nil_state);
	do {
		if (curr < dyn_root) {
			if (pos < end) {
				size_t next = nfa->loop_state_move<TR>(curr, pos, end, tr);
				if (nfa->is_term(curr)) {
					find_hits(nfa, curr, &ctx->hits, matched);
				}
				curr = next;
			}
			else {
				assert(pos == end);
				if (nfa->is_term(curr)) {
					find_hits(nfa, curr, &ctx->hits, matched);
				}
				break;
			}
		}
		else {
			auto try_hit_curr = [&]() {
				if (this->is_term(curr - dyn_root)) {
					get_subset(curr, &ctx->matchid_states);
					for (size_t matchState : ctx->matchid_states)
						find_hits(nfa, matchState, &ctx->hits, matched);
				}
			};
			if (pos < end) {
				size_t next;
				do next = dyn_state_move(nfa, curr, (byte_t)tr(*pos));
				while (++pos < end && next == curr);
				try_hit_curr();
				curr = next;
			}
			else {
				assert(pos == end);
				try_hit_curr();
				break;
			}
		}
	} while (nil_state != curr);

	if (terark_unlikely(dyn_mem_size() > maxmem)) {
		dyn_discard_deep_cache();
	}
	m_max_partial_match_len = pos - text.udata();
	collect_hits(&ctx->hits, matched);
	return matched->size();
}

template<class TR, class DFA>
size_t
DenseDFA_DynDFA_256::full_match_all_len_with_tr(
		TR tr, const DFA* nfa,
		fstring text,
		valvec<MultiRegexFullMatch::LenRegexID>* matched,
		DynDFA_Context* ctx)
{
	assert(power_set.size() >= 2);
	size_t curr = dyn_root;
	const byte_t* pos = text.udata();
	const byte_t* end = pos + text.size();
	auto& tls = g_MatchStateThreadLocal;
	tls.clean_reset();
	do {
		size_t full = dyn_full_match_root(nfa, curr);
		if (nil_state != full) {
			get_subset(full, &ctx->matchid_states);
			for (size_t matchState: ctx->matchid_states)
				tls.m_pos_state_vec.push_back
					({int(pos - text.udata()), int(matchState)});
		}
		if (pos < end) {
			size_t next;
			do next = dyn_state_move(nfa, curr, (byte_t)tr(*pos));
			while (++pos < end && next == curr);
			curr = next;
		}
		else {
			assert(pos == end);
			break;
		}
	} while (nil_state != curr);
	m_max_partial_match_len = pos - text.udata();
	if (terark_unlikely(dyn_mem_size() > maxmem)) {
		dyn_discard_deep_cache();
	}
	tls.collect(nfa, matched);
	return matched->size();
}

template<class TR>
size_t
DenseDFA_DynDFA_256::full_match_all_len_with_tr(
		TR tr, const VirtualMachineDFA* nfa,
		fstring text,
		valvec<MultiRegexFullMatch::LenRegexID>* matched,
		DynDFA_Context* ctx)
{
	assert(power_set.size() >= 2);
	size_t curr = dyn_root;
	const byte_t* pos = text.udata();
	const byte_t* end = pos + text.size();
	BOOST_STATIC_ASSERT(VirtualMachineDFA::nil_state == nil_state);
	auto& tls = g_MatchStateThreadLocal;
	tls.clean_reset();
	do {
		if (curr < dyn_root) {
			if (pos < end) {
				size_t next = nfa->loop_state_move<TR>(curr, pos, end, tr);
				if (nfa->is_term(curr)) {
					const byte_t* hitpos = next == curr ? pos : pos - 1;
					tls.m_pos_state_vec.push_back
						({int(hitpos - text.udata()), int(curr)});
				}
				curr = next;
			}
			else {
				assert(pos == end);
				if (nfa->is_term(curr)) {
					tls.m_pos_state_vec.push_back
						({int(pos - text.udata()), int(curr)});
				}
				break;
			}
		}
		else {
			auto try_hit_curr = [&](const byte_t* pos) {
				if (this->is_term(curr - dyn_root)) {
					get_subset(curr, &ctx->matchid_states);
					for (size_t matchState : ctx->matchid_states)
						tls.m_pos_state_vec.push_back
							({int(pos - text.udata()), int(matchState)});
				}
			};
			if (pos < end) {
				size_t next;
				do next = dyn_state_move(nfa, curr, (byte_t)tr(*pos));
				while (++pos < end && next == curr);
				try_hit_curr(next == curr ? pos : pos - 1);
				curr = next;
			}
			else {
				assert(pos == end);
				try_hit_curr(pos);
				break;
			}
		}
	} while (nil_state != curr);
	m_max_partial_match_len = pos - text.udata();
	if (terark_unlikely(dyn_mem_size() > maxmem)) {
		dyn_discard_deep_cache();
	}
	tls.collect(nfa, matched);
	return matched->size();
}

template<class DFA>
class MultiRegexFullMatchDynamicDfaTmpl : public MultiRegexFullMatch {
public:
	MultiRegexFullMatchDynamicDfaTmpl(const DFA* dfa, bool /*dyna*/) {
		m_dfa = dfa;
		m_dyn = new DenseDFA_DynDFA_256;
		m_dyn->dyn_init(dfa);
	}

	DynDFA_Context m_ctx;
	template<class TR>
	size_t match_with_tr(fstring text, TR tr) {
		size_t fmlen = m_dyn->template full_match_with_tr<TR>
			(tr, static_cast<const DFA*>(m_dfa), text, &m_regex_idvec, &m_ctx);
		m_max_partial_match_len = m_dyn->m_max_partial_match_len;
		return fmlen;
	}
	template<class TR>
	size_t shortest_match_with_tr(fstring text, TR tr) {
		size_t fmlen = m_dyn->template shortest_full_match_with_tr<TR>
			(tr, static_cast<const DFA*>(m_dfa), text, &m_regex_idvec, &m_ctx);
		m_max_partial_match_len = m_dyn->m_max_partial_match_len;
		return fmlen;
	}

	template<class TR>
	size_t match_all_with_tr(fstring text, TR tr) {
		size_t num;
        const DFA* au = static_cast<const DFA*>(m_dfa);
        if (au->m_atom_dfa_num < m_options->maxBitmapSize) {
            num = m_dyn->template full_match_all_with_tr_bitmap<TR>
			                (tr, au, text, &m_regex_idvec, &m_ctx);
        }
        else {
            num = m_dyn->template full_match_all_with_tr_sort_uniq<TR>
			                (tr, au, text, &m_regex_idvec, &m_ctx);
        }
		m_max_partial_match_len = m_dyn->m_max_partial_match_len;
		return num;
	}

	template<class TR>
	size_t match_all_len_with_tr(fstring text, TR tr) {
		const DFA* au = static_cast<const DFA*>(m_dfa);
		m_ctx.hits.erase_all();
		m_all_match.erase_all();
		m_cur_match.erase_all();
		m_regex_idvec.erase_all();
		size_t num = m_dyn->template full_match_all_len_with_tr<TR>
						(tr, au, text, &m_cur_match, &m_ctx);
		m_max_partial_match_len = m_dyn->m_max_partial_match_len;
		return num;
	}

	terark_flatten
	size_t match(fstring text) override {
		return match_with_tr(text, IdentityTR());
	}
	terark_flatten
	size_t match(fstring text, const ByteTR& tr) override {
		return match_with_tr<const ByteTR&>(text, tr);
	}
	terark_flatten
	size_t match(fstring text, const byte_t* tr) override {
		return match_with_tr(text, TableTranslator(tr));
	}

	terark_flatten
	size_t shortest_match(fstring text) override {
		return shortest_match_with_tr(text, IdentityTR());
	}
	terark_flatten
	size_t shortest_match(fstring text, const ByteTR& tr) override {
		return shortest_match_with_tr<const ByteTR&>(text, tr);
	}
	terark_flatten
	size_t shortest_match(fstring text, const byte_t* tr) override {
		return shortest_match_with_tr(text, TableTranslator(tr));
	}

	terark_flatten
	size_t match_all(fstring text) override {
		return match_all_with_tr(text, IdentityTR());
	}
	terark_flatten
	size_t match_all(fstring text, const ByteTR& tr) override {
		return match_all_with_tr<const ByteTR&>(text, tr);
	}
	terark_flatten
	size_t match_all(fstring text, const byte_t* tr) override {
		return match_all_with_tr<TableTranslator>(text, tr);
	}

	terark_flatten
	size_t match_all_len(fstring text) override {
		return match_all_len_with_tr(text, IdentityTR());
	}
	terark_flatten
	size_t match_all_len(fstring text, const ByteTR& tr) override {
		return match_all_len_with_tr<const ByteTR&>(text, tr);
	}
	terark_flatten
	size_t match_all_len(fstring text, const byte_t* tr) override {
		return match_all_len_with_tr<TableTranslator>(text, tr);
	}

	bool has_hit(int regex_id) const override {
		const DFA* au = static_cast<const DFA*>(m_dfa);
		if (au->m_atom_dfa_num < m_options->maxBitmapSize) {
			return size_t(regex_id) < m_ctx.hits.size() && m_ctx.hits.is1(regex_id);
		} else {
			return MultiRegexFullMatch::has_hit(regex_id);
		}
	}

	void clear_match_result() override {
		MultiRegexFullMatch::clear_match_result();
		m_ctx.hits.erase_all();
	}

	MultiRegexFullMatch* clone() const override {
		return new MultiRegexFullMatchDynamicDfaTmpl(*this);
	}
};

// defined in mre_ac.cpp:
MultiRegexFullMatch* MultiRegexFullMatch_ac(const BaseDFA* dfa);

// defined in mre_dawg.cpp:
MultiRegexFullMatch* MultiRegexFullMatch_dawg(const BaseDFA* dfa);

MultiRegexFullMatch* MultiRegexFullMatch::create(const MultiRegexMatchOptions& opt) {
	const BaseDFA* dfa = opt.get_dfa();
	if (!dfa) {
		THROW_STD(invalid_argument,
			"dfa is NULL, dfaFilePath = %s", opt.dfaFilePath.c_str());
	}
    if (MultiRegexFullMatch* p = MultiRegexFullMatch_ac(dfa)) {
        return p;
    }
    if (MultiRegexFullMatch* p = MultiRegexFullMatch_dawg(dfa)) {
        return p;
    }
	bool is_dyn_dfa;
	if (auto vmdfa = dynamic_cast<const VirtualMachineDFA*>(dfa))
		is_dyn_dfa = vmdfa->is_dead(initial_state);
	else
		is_dyn_dfa = dfa->v_state_move(initial_state, 258) != dfa->v_nil_state();

    size_t cluster_num = size_t(-1);
#define GetClusterNum(MyClassName, DFA) \
	else if (auto au = dynamic_cast<const DFA*>(dfa)) { \
        cluster_num = au->m_dfa_cluster_num; \
	}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    DispatchConcreteDFA(~, GetClusterNum);
    assert(size_t(-1) != cluster_num);
#undef GetClusterNum

	std::unique_ptr<MultiRegexFullMatch> fm;
#define CreateFullMatchTpl(MyClassName, DFA) \
	else if (auto au = dynamic_cast<const DFA*>(dfa)) { \
		fm.reset(new MyClassName<DFA >(au, is_dyn_dfa)); \
	}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	if (is_dyn_dfa && opt.enableDynamicDFA && cluster_num >= 2) {
		DispatchConcreteDFA(MultiRegexFullMatchDynamicDfaTmpl, CreateFullMatchTpl);
	} else {
		DispatchConcreteDFA(MultiRegexFullMatchTmpl, CreateFullMatchTpl);
	}
	fm->m_options = &opt;
	return fm.release();
}

MultiRegexFullMatch::MultiRegexFullMatch(const MultiRegexFullMatch& y)
  : m_regex_idvec(y.m_regex_idvec)
  , m_all_match(y.m_all_match)
  , m_dfa(y.m_dfa)
  , m_dyn(y.m_dyn)
{
	if (m_dyn)
		m_dyn->refcnt++;
}

MultiRegexFullMatch::MultiRegexFullMatch() {
	m_dfa = NULL;
	m_dyn = NULL;
	m_max_partial_match_len = 0;
}
MultiRegexFullMatch::~MultiRegexFullMatch() {
	if (m_dyn && 0 == --m_dyn->refcnt)
		delete m_dyn;
}

void MultiRegexFullMatch::set_maxmem(size_t maxmem) {
	if (m_dyn)
		m_dyn->maxmem = maxmem;
}

size_t MultiRegexFullMatch::get_maxmem() const {
	if (m_dyn)
		return m_dyn->maxmem;
	return 0;
}

void MultiRegexFullMatch::warm_up() {
	if (m_dyn)
		m_dyn->warm_up(m_dfa);
}

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
template<class DFA>
class MultiRegexSubmatchTwoPassTmpl : public MultiRegexSubmatch {
public:
	// RootState(regex_id) = regex_id + 1
	template<class TR>
	size_t match_with_tr(fstring text, TR tr) {
		const DFA* dfa = static_cast<const DFA*>(this->dfa);
		assert(NULL != this->m_first_pass);
		size_t match_len;
		if (m_first_pass->get_dyn_dfa()) {
			// dynamic dfa is not pathzipped
			assert(dfa->num_zpath_states() == 0);
			match_len = static_cast<MultiRegexFullMatchDynamicDfaTmpl<DFA>*>
				(m_first_pass)->template match_with_tr<TR>(text, tr);
		}
		else {
			match_len = static_cast<MultiRegexFullMatchTmpl<DFA>*>
				(m_first_pass)->template match_with_tr<TR>(text, tr);
		}
		text.n = match_len;
		m_fullmatch_regex.swap(m_first_pass->mutable_regex_idvec());
		assert(this->m_fullmatch_regex.size() || 0 == match_len);
		for(int  regex_id : this->m_fullmatch_regex) {
			int* pcap = cap_pos_ptr[regex_id];
			this->reset(regex_id);
			size_t root;
			if (this->cap_pos_ptr.size() == 2) // only 1 regex
				root = initial_state; // regex[0] == regex[all]
			else
				root = regex_id + 1;
			// intentional not use return value
			get_capture_pos_with_word_boundary<TR>(dfa, root, regex_id, text, tr);
			pcap[0] = 0;
			pcap[1] = match_len;
		}
		return match_len;
	}

	template<class TR>
	size_t get_capture_pos_with_word_boundary
	(const DFA* dfa, size_t root, int regex_id, fstring text, TR tr)
	const {
		int* const pcap = cap_pos_ptr[regex_id];
		size_t curr = root;
		size_t pos = 0;
		auto set_capture_pos = [&](size_t, fstring val, size_t) {
			TERARK_ASSERT_EQ(val.size(), sizeof(int32_t));
			int32_t capture_id = unaligned_load<int32_t>(val.data());
		//	printf("j=%zd capture_id=%d  text=%.*s\n", j, capture_id, text.ilen(), text.p);
			TERARK_ASSERT_GE(capture_id, 2);
			TERARK_ASSERT_LT(pcap + capture_id, this->cap_pos_ptr[regex_id+1]);
			pcap[capture_id] = int(pos);
		};
		BOOST_STATIC_ASSERT(sizeof(size_t) >= 4);
		const int Bufsize = sizeof(size_t); // CPU word size
		ForEachWord_DFS_FixedBuffer<const DFA&, decltype(set_capture_pos), Bufsize>
		vis(*dfa, NULL, set_capture_pos);
		for (; ; ++pos) {
			if (dfa->is_pzip(curr)) {
				fstring zstr = dfa->get_zpath_data(curr, nullptr);
				size_t minlen = std::min(zstr.size(), text.size() - pos);
				for(size_t j = 0; j < minlen; ++j, ++pos) {
					if ((byte_t)tr(text.uch(pos)) != zstr.uch(j))
						return pos;
				}
			}
			size_t value_start = dfa->state_move(curr, SUB_MATCH_DELIM);
			if (DFA::nil_state != value_start) {
				if (dfa_check_empty_width(dfa, text, curr, pos)) {
					vis.start(value_start, 0); // will call set_capture_pos at least once
				}
			}
			if (text.size() == pos)
				return pos;
			size_t next = dfa->state_move(curr, (byte_t)tr(text.uch(pos)));
			if (DFA::nil_state == next)
				return pos;
			TERARK_ASSERT_LT(next, dfa->total_states());
			ASSERT_isNotFree2(dfa, next);
			curr = next;
		}
		TERARK_DIE("Should not goes here");
	}

	template<class TR>
	size_t shortest_match_with_tr(fstring text, TR tr) {
		THROW_STD(invalid_argument, "Currently not supported");
	}

	terark_flatten
	size_t match(fstring text) override {
		return match_with_tr(text, IdentityTR());
	}
	terark_flatten
	size_t match(fstring text, const ByteTR& tr) override {
		return match_with_tr<const ByteTR&>(text, tr);
	}
	terark_flatten
	size_t match(fstring text, const byte_t* tr) override {
		return match_with_tr(text, TableTranslator(tr));
	}

	MultiRegexSubmatch* clone() const override {
		return new MultiRegexSubmatchTwoPassTmpl(*this);
	}
};

template<>
class MultiRegexSubmatchTwoPassTmpl<VirtualMachineDFA>
	: public MultiRegexSubmatch {
public:
	// RootState(regex_id) = regex_id + 1
	template<class TR>
	size_t match_with_tr(fstring text, TR tr) {
		auto dfa = static_cast<const VirtualMachineDFA*>(this->dfa);
		assert(NULL != this->m_first_pass);
		size_t match_len;
		if (m_first_pass->get_dyn_dfa()) {
			// dynamic dfa is not pathzipped
			assert(dfa->num_zpath_states() == 0);
			match_len = static_cast<MultiRegexFullMatchDynamicDfaTmpl<VirtualMachineDFA>*>
				(m_first_pass)->template match_with_tr<TR>(text, tr);
		}
		else {
			match_len = static_cast<MultiRegexFullMatchTmpl<VirtualMachineDFA>*>
				(m_first_pass)->template match_with_tr<TR>(text, tr);
		}
		this->m_fullmatch_regex.swap(m_first_pass->mutable_regex_idvec());
		assert(this->m_fullmatch_regex.size() || 0 == match_len);
		for(int  regex_id : this->m_fullmatch_regex) {
			int* pcap = cap_pos_ptr[regex_id];
			assert(regex_id + 1 < (int)dfa->num_roots());
			size_t curr;
			if (dfa->num_roots() == 2) // only 1 regex
				 curr = dfa->get_root(0); // regex[0] == regex[all]
			else curr = dfa->get_root(regex_id + 1);
			this->reset(regex_id);
			auto pos = reinterpret_cast<const byte_t*>(text.p);
			auto end = pos + match_len;
			size_t offset;
			do {
				size_t next;
				if (dfa->is_pzip(curr)) {
					fstring zstr = dfa->get_zpath_data(curr, NULL);
					assert(pos + zstr.size() <= end);
					pos += zstr.size();
					offset = pos - text.udata();
					next = dfa->state_move(curr, (byte_t)tr(*pos++));
				}
				else {
					next = dfa->loop_state_move<TR>(curr, pos, end, tr);
					if (curr == next)
						 offset = pos - text.udata();
					else offset = pos - text.udata() - 1;
				}
				if (dfa->has_capture(curr)) {
					/// ([0-9]+)([a-z]+)
					///        ^^----------- here capture cnt == 2
					const byte_t* capt = dfa->get_capture(curr);
					const size_t cnt = capt[0] + 1;
					size_t j = 0;
					bool pos_check_ok = true;
					if (255 == capt[1+j]) { // REGEX_DFA_WORD_BOUNDARY
						pos_check_ok = text.is_word_boundary(offset);
						j++;
					}
					else if (254 == capt[1+j]) { // REGEX_DFA_NON_WORD_BOUNDARY
						pos_check_ok = !text.is_word_boundary(offset);
						j++;
					}
					if (pos_check_ok) {
						for(; j < cnt; ++j) {
							size_t capture_id = capt[1+j] + 2;
							assert(pcap + capture_id < cap_pos_ptr[regex_id+1]);
							pcap[capture_id] = (int)offset;
						}
					}
				}
				curr = next;
			} while (offset < match_len && dfa->nil_state != curr);
			pcap[0] = 0;
			pcap[1] = match_len;
		}
		return match_len;
	}

	template<class TR>
	size_t shortest_match_with_tr(fstring text, TR tr) {
		THROW_STD(invalid_argument, "Currently not supported");
	}

	terark_flatten
	size_t match(fstring text) override {
		return match_with_tr(text, IdentityTR());
	}
	terark_flatten
	size_t match(fstring text, const ByteTR& tr) override {
		return match_with_tr<const ByteTR&>(text, tr);
	}
	terark_flatten
	size_t match(fstring text, const byte_t* tr) override {
		return match_with_tr(text, TableTranslator(tr));
	}

	MultiRegexSubmatch* clone() const override {
		return new MultiRegexSubmatchTwoPassTmpl(*this);
	}
};

MultiRegexSubmatch*
MultiRegexSubmatch::create(const MultiRegexMatchOptions& opt) {
	const BaseDFA* dfa = opt.get_dfa();
	if (!dfa) {
		THROW_STD(invalid_argument,
			"dfa is NULL, dfaFilePath = %s", opt.dfaFilePath.c_str());
	}
	if (opt.regexMetaFilePath.empty()) {
		THROW_STD(invalid_argument,
			"MultiRegexMatchOptions::regexMetaFilePath can not be empty");
	}
	Auto_close_fp regexMetaFile(fopen(opt.regexMetaFilePath.c_str(), "r"));
	if (!regexMetaFile) {
		THROW_STD(invalid_argument, "ERROR: fopen(regexMetaFile = %s, r) = %s"
			, opt.regexMetaFilePath.c_str(), strerror(errno));
	}
	std::unique_ptr<MultiRegexSubmatch> p;
#define CreateSubmatchObj(ClassName, DFA) \
	else if (dynamic_cast<const DFA*>(dfa)) { \
		p.reset(new ClassName<DFA >()); \
	}
	DispatchConcreteDFA(MultiRegexSubmatchTwoPassTmpl, CreateSubmatchObj)
	p->dfa = dfa;
	p->m_first_pass = MultiRegexFullMatch::create(opt);
	terark::LineBuf line;
	valvec<fstring> F;
	valvec<int> offsets;
	int total_caps = 0;
	while (line.getline(regexMetaFile) > 0) {
		line.chomp();
		line.split('\t', &F);
	//	int regex_id   = terark::lcast(F[0]);
		int n_submatch = terark::lcast(F[1]);
		int is_onepass = terark::lcast(F[2]);
		offsets.push_back(total_caps);
		total_caps += is_onepass ? 2*n_submatch : 2;
	}
	offsets.push_back(total_caps);
	p->cap_pos_data.resize_no_init(total_caps);
	p->cap_pos_ptr.resize(offsets.size());
	for (size_t i = 0; i < offsets.size(); ++i)
		p->cap_pos_ptr[i] = p->cap_pos_data.data() + offsets[i];

//	printf("MultiRegexSubmatch::init: cap_pos_data.size()=%zd\n", p->cap_pos_data.size());
//	printf("MultiRegexSubmatch::init: cap_pos_ptr.size()=%zd\n", p->cap_pos_ptr.size());

	return p.release();
}

void MultiRegexSubmatch::warm_up() {
	if (m_first_pass && m_first_pass->get_dyn_dfa())
		m_first_pass->warm_up();
}

} // namespace terark

#include "create_regex_dfa_impl.hpp"
