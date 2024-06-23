#pragma once

#include "fsa.hpp"
#include "nfa.hpp"
#include "automata.hpp"
#include <terark/util/throw.hpp>
#include <terark/gold_hash_map.hpp>
#include <boost/noncopyable.hpp>

namespace terark {

template<class DFA>
class NFA_ForMultiRootDFA : boost::noncopyable {
	const DFA* dfa;
	valvec<size_t> roots;
public:
	enum { sigma = DFA::sigma };
	typedef typename DFA::state_id_t state_id_t;
	static const state_id_t nil_state = DFA::nil_state;
	static const state_id_t max_state = DFA::max_state;

	explicit NFA_ForMultiRootDFA(const DFA* dfa1) : dfa(dfa1) {}
	size_t get_sigma() const { return sigma; }
	size_t total_states() const { return dfa->total_states(); }

	bool is_final(size_t s) const {
		assert(s < dfa->total_states());
		return dfa->is_term(s);
	}
	void get_epsilon_targets(size_t s, valvec<state_id_t>* children) const {
		assert(s < dfa->total_states());
		children->erase_all();
		if (initial_state == s) {
			for (size_t root : roots)
				children->push_back(root);
		}
	}
	void get_non_epsilon_targets(size_t s, valvec<CharTarget<size_t> >* children)
	const {
		assert(s < dfa->total_states());
		dfa->get_all_move(s, children);
	}
	void add_root(size_t root) { roots.push_back(root); }

	typedef NFA_ForMultiRootDFA MyType;
	#include "ppi/nfa_algo.hpp"
};

// When treated as an NFA, only initial_state is nondeterminized
//   1. NFA::make_dfa can be applied on this "NFA" to get a unioned
//      DFA of all sub DFAs
//
// When treated as an DFA, this DFA has multiple individule sub DFAs
//   1. This "DFA" can be minimized by general DFA minimization algorithms,
//      such as Hopcroft, Lei Peng's DFA_Algorithm::adfa_minimize
//   2. In general, DFA minimization algorithm could only merge tails of
//      multiple sub DFAs
//   3. If sub DFA1 is equivalent to sub DFA2, after adfa_minimize, the root2
//      will have two identical elements corresponding to DFA1 and DFA2
//   4. Calling NFA::make_dfa after adfa_minimize to create the union-dfa
//      will significantly reduce memory and running time
class TERARK_DLL_EXPORT FSA_ForUnionDFA : boost::noncopyable {
	mutable valvec<size_t> m_stack_dests;
	mutable valvec<CharTarget<size_t> > m_stack_moves;
	valvec<uint32_t> m_index;
	valvec<const BaseDFA*> m_dfas;
	size_t m_total_states;
	size_t m_max_sigma;
	size_t m_free_states;
	bool   m_is_owner;

public:
	typedef uint32_t state_id_t;
	enum { sigma = 512 };
	static const state_id_t nil_state = uint32_t(-1);
	static const state_id_t max_state = uint32_t(-1) - 1;

	FSA_ForUnionDFA();
	~FSA_ForUnionDFA();

	void clear();
	void set_own(bool v) { m_is_owner = v; }
	void add_dfa(const BaseDFA* dfa);
	void get_all_root(valvec<size_t>* roots) const;
	void compile();

	// NFA methods:
	size_t get_sigma() const { return m_max_sigma; }
	size_t total_states() const { return m_total_states; }
	bool is_final(size_t s) const;
	void get_epsilon_targets(size_t s, valvec<state_id_t>* children) const;
	void get_non_epsilon_targets(size_t s, valvec<CharTarget<size_t> >* children) const;

	// DFA methods: the FSA is DFA except initial_state
	// the DFA has multiple roots, and can be minimized by Hopcroft/adfa_minimize
	//
	size_t get_all_dest(size_t s, size_t* dests) const;
	size_t get_all_move(size_t s, CharTarget<size_t>* moves) const;

	template<class OP>
	void for_each_dest(size_t s, OP op) const {
		size_t cnt;
		size_t oldsize = risk_get_all_dest(s, &cnt);
		for (size_t j = oldsize; j < oldsize + cnt; ++j) {
			op(m_stack_dests[j]);
		}
		m_stack_dests.risk_set_size(oldsize);
	}

	template<class OP>
	void for_each_dest_rev(size_t s, OP op) const {
		size_t cnt;
		size_t oldsize = risk_get_all_dest(s, &cnt);
		for (size_t j = oldsize + cnt; j > oldsize; ) {
			--j;
			op(m_stack_dests[j]);
		}
		m_stack_dests.risk_set_size(oldsize);
	}

	template<class OP>
	void for_each_move(size_t s, OP op) const {
		size_t cnt;
		size_t oldsize = risk_get_all_move(s, &cnt);
		for (size_t i = oldsize; i < oldsize + cnt; ++i) {
			CharTarget<size_t> ct = m_stack_moves[i];
			op(ct.target, ct.ch);
		}
		m_stack_moves.risk_set_size(oldsize);
	}

	size_t risk_get_all_dest(size_t s, size_t* pcnt) const;
	size_t risk_get_all_move(size_t s, size_t* pcnt) const;

	size_t num_zpath_states() const { return 0; }
	bool is_dag() const;
	bool is_free(size_t s) const;
	bool is_term(size_t s) const { return is_final(s); }
	bool is_pzip(size_t  ) const { return false; }
	bool has_freelist() const { return false; }

	size_t num_used_states() const { return m_total_states - m_free_states; }
	size_t num_free_states() const { return m_free_states; }
	fstring get_zpath_data(size_t, MatchContext*) const { return ""; }

	// for adfa_minimize
	size_t adfa_hash_hash(const state_id_t* Min, size_t state_id) const;
	bool   adfa_hash_equal(const state_id_t* Min, size_t x, size_t y) const;

typedef FSA_ForUnionDFA MyType;
	friend size_t
	GraphTotalVertexes(const MyType* au) { return au->total_states(); }
#include "ppi/post_order.hpp"
#include "ppi/adfa_minimize.hpp"
#include "ppi/dfa_hopcroft.hpp"
#include "ppi/nfa_algo.hpp"
};

class TERARK_DLL_EXPORT BaseLazyUnionDFA : public MatchingDFA {
public:
	virtual size_t find_sub_dfa(fstring) const = 0;
};

TERARK_DLL_EXPORT
BaseLazyUnionDFA* createLazyUnionDFA(const MatchingDFA** ppDFA, size_t num, bool own);

TERARK_DLL_EXPORT
MatchingDFA* loadAsLazyUnionDFA(fstring memory, bool ordered);

} // namespace terark
