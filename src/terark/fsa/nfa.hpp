#pragma once

#include <terark/config.hpp>
#include <terark/fstring.hpp>
#include <terark/gold_hash_map.hpp>
#include <terark/valvec.hpp>
#include <terark/bitmap.hpp>
#include <terark/smallmap.hpp>
#include <terark/util/autoclose.hpp>
#include <terark/util/profiling.hpp>
#include <terark/util/throw.hpp>
#include <stdio.h>
#include <errno.h>
#include "fsa.hpp"
#include "graph_walker.hpp"
#include "power_set.hpp"

namespace terark {

struct NFA_Transition4B {
	NFA_Transition4B(uint32_t target, auchar_t ch) {
		this->target = target;
		this->ch     = ch;
	}
	uint32_t target : 23;
	unsigned ch     :  9;
	typedef uint32_t state_id_t;
	enum { sigma = 512 };
};

template<class Transition = NFA_Transition4B>
class GenericNFA {
public:
	typedef typename Transition::state_id_t state_id_t;
protected:
	struct CharLess {
		bool operator()(const Transition& x, state_id_t y) const { return x.ch < y; }
		bool operator()(state_id_t x, const Transition& y) const { return x < y.ch; }
	};
	struct NfaState {
		valvec<Transition> targets;
	   	// first n_epsilon elements are epsilon transitions
		// 'ch' of epsilon transtions are ignored
		size_t    n_epsilon : 8*sizeof(size_t)-1;
		unsigned  b_final   : 1;
		NfaState() { n_epsilon = 0; b_final = 0; }
	};
	valvec<NfaState> states;

public:
	typedef std::pair<const Transition*, const Transition*> target_range_t;
	typedef std::pair<      Transition*,       Transition*> target_range_tm; // mutable
	enum { sigma = Transition::sigma };

	GenericNFA() {
		states.resize(1); // make initial_state
	}

	size_t get_sigma() const { return sigma; }

	size_t total_states() const { return states.size(); }
	void erase_all() { states.erase_all(); }
	void resize_states(size_t n) { states.resize(n); }

	state_id_t new_state() {
	   	state_id_t s = states.size();
	   	states.push_back();
	   	return s;
   	}

	// if trans has extra complex members, use this function
	void add_move(state_id_t source, const Transition& trans) {
		assert(source       < states.size());
		assert(trans.target < states.size());
		valvec<Transition>& t = states[source].targets;
		auto pos = std::upper_bound(t.begin(), t.end(), trans.ch, CharLess());
		t.insert(pos, trans);
	}
	void add_move(state_id_t source, state_id_t target, auchar_t ch) {
		assert(source < states.size());
		assert(target < states.size());
		valvec<Transition>& t = states[source].targets;
		auto pos = std::upper_bound(t.begin(), t.end(), ch, CharLess());
		t.insert(pos, Transition(target, ch));
	}

	void add_epsilon(state_id_t source, const Transition& trans) {
		assert(source       < states.size());
		assert(trans.target < states.size());
		NfaState& s = states[source];
		s.targets.insert(s.n_epsilon, trans);
		s.n_epsilon++;
	}
	void add_epsilon(state_id_t source, state_id_t target) {
		assert(source < states.size());
		assert(target < states.size());
		NfaState& s = states[source];
		s.targets.insert(s.n_epsilon, Transition(target, 0));
		s.n_epsilon++;
	}

	bool is_final(state_id_t state) const {
		assert(state < states.size());
		return states[state].b_final;
	}

	void set_final(state_id_t state) {
		assert(state < states.size());
		states[state].b_final = 1;
	}

	void get_epsilon_targets(state_id_t s, valvec<state_id_t>* children) const {
		assert(s < states.size());
		const NfaState& ts = states[s];
		children->erase_all();
		children->reserve(ts.n_epsilon);
		for (size_t i = 0; i < ts.n_epsilon; ++i)
			children->unchecked_push_back(ts.targets[i].target);
	}
	void get_non_epsilon_targets(state_id_t s, valvec<CharTarget<size_t> >* children) const {
		assert(s < states.size());
		const NfaState& ts = states[s];
		children->erase_all();
		children->reserve(ts.targets.size() - ts.n_epsilon);
		for (size_t i = ts.n_epsilon; i < ts.targets.size(); ++i)
			children->unchecked_push_back(
				CharTarget<size_t>(ts.targets[i].ch, ts.targets[i].target));
	}
typedef GenericNFA MyType;
#include "ppi/nfa_algo.hpp"
#include "ppi/nfa_mutation_algo.hpp"
};

// a NFA which is derived from a DFA, there are few states of the NFA
// are Non-Determinal, most states are determinal
// Non-Determinal states are save in a hash map
template<class DFA>
class NFA_BaseOnDFA {
public:
	typedef typename DFA::state_id_t state_id_t;
	enum { sigma = DFA::sigma };

	DFA* dfa;
	gold_hash_map<state_id_t, valvec<state_id_t> > epsilon;
	gold_hash_map<state_id_t, valvec<CharTarget<state_id_t> > > non_epsilon;

public:
	NFA_BaseOnDFA(DFA* dfa1) : dfa(dfa1) {}

	size_t get_sigma() const { return sigma; }

	NFA_BaseOnDFA& reset(DFA* dfa1) {
	   	dfa = dfa1;
		this->erase_all();
		dfa1->erase_all();
		return *this;
	}
	NFA_BaseOnDFA& reset() {
		this->erase_all();
		dfa ->erase_all();
		return *this;
	}
	void erase_all() {
	   	epsilon.erase_all();
	   	non_epsilon.erase_all();
   	}
	void resize_states(size_t n) { dfa->resize_states(n); }

	size_t total_states() const { return dfa->total_states(); }

	state_id_t new_state() { return dfa->new_state(); }

	/// will not add duplicated (targets, ch)
	/// @param ch the transition label char
	void add_move(state_id_t source, state_id_t target, state_id_t ch) {
		assert(ch < sigma);
		state_id_t existed_target = dfa->add_move(source, target, ch);
		if (DFA::nil_state == existed_target || target == existed_target)
			return; // Good luck, it is determinal
		auto& targets = non_epsilon[source];
		CharTarget<state_id_t> char_target(ch, target);
		auto  ins_pos = std::lower_bound(targets.begin(), targets.end(), char_target);
		if (targets.end() == ins_pos || char_target < *ins_pos)
			targets.insert(ins_pos, char_target);
	}
	void push_move(state_id_t source, state_id_t target, state_id_t ch) {
		assert(ch < sigma);
		state_id_t existed_target = dfa->add_move(source, target, ch);
		if (DFA::nil_state == existed_target || target == existed_target)
			return; // Good luck, it is determinal
		auto& targets = non_epsilon[source];
		targets.push_back(CharTarget<state_id_t>(ch, target));
	}

	/// will not add duplicated targets
	void add_epsilon(state_id_t source, state_id_t target) {
		auto& eps = epsilon[source];
		auto  ins_pos = std::lower_bound(eps.begin(), eps.end(), target);
		if (eps.end() == ins_pos || target < *ins_pos) {
		//	fprintf(stderr, "add_epsilon: source=%u target=%u\n", source, target);
			eps.insert(ins_pos, target);
		}
	}

	/// may add duplicated targets
	void push_epsilon(state_id_t source, state_id_t target) {
		assert(source < dfa->total_states());
		assert(target < dfa->total_states());
		epsilon[source].push_back(target);
	}

	void sort_unique_shrink_all() {
		for (size_t i = 0; i < epsilon.end_i(); ++i) {
			auto& eps = epsilon.val(i);
			std::sort(eps.begin(), eps.end());
			eps.trim(std::unique(eps.begin(), eps.end()));
			eps.shrink_to_fit();
		}
		for (size_t i = 0; i < non_epsilon.end_i(); ++i) {
			auto& tar = non_epsilon.val(i);
			std::sort(tar.begin(), tar.end());
			tar.trim(std::unique(tar.begin(), tar.end()));
			tar.shrink_to_fit();
		}
		epsilon.shrink_to_fit();
		non_epsilon.shrink_to_fit();
		dfa->shrink_to_fit();
	}

	bool is_final(state_id_t s) const {
		assert(s < dfa->total_states());
		return dfa->is_term(s);
	}
	void set_final(state_id_t s) {
		assert(s < dfa->total_states());
		dfa->set_term_bit(s);
	}

	void get_epsilon_targets(state_id_t s, valvec<state_id_t>* children) const {
		assert(s < dfa->total_states());
		size_t idx = epsilon.find_i(s);
		if (epsilon.end_i() == idx) {
			children->erase_all();
		} else {
			auto& eps = epsilon.val(idx);
			children->assign(eps.begin(), eps.end());
		//	fprintf(stderr, "get_epsilon_targets(%u):", s);
		//	for (state_id_t t : epsilon) fprintf(stderr, " %u", t);
		//	fprintf(stderr, "\n");
		}
	}
	void get_non_epsilon_targets(state_id_t s, valvec<CharTarget<size_t> >* children) const {
		assert(s < dfa->total_states());
		size_t idx = non_epsilon.find_i(s);
		dfa->get_all_move(s, children);
		if (non_epsilon.end_i() != idx) {
			auto& targets = non_epsilon.val(idx);
			size_t n_xxx = targets.size();
			size_t n_dfa = children->size();
			children->resize_no_init(n_dfa + n_xxx);
			auto src = targets.begin();
			auto dst = children->begin() + n_dfa;
			for (size_t i = 0; i < n_xxx; ++i) {
				auto& x = dst[i];
				auto& y = src[i];
				x.ch = y.ch;
				x.target = y.target;
			}
		}
	}

	void concat(state_id_t root1, state_id_t root2) {
		PFS_GraphWalker<state_id_t> walker;
		walker.resize(dfa->total_states());
		walker.putRoot(root1);
		while (!walker.is_finished()) {
			state_id_t curr = walker.next();
			if (dfa->is_term(curr)) {
				dfa->clear_term_bit(curr);
				add_epsilon(curr, root2);
			}
			walker.putChildren(dfa, curr);
		}
	}

	void suffix_dfa_add_epsilon_on_utf8() {
		valvec<state_id_t> stack;
		febitvec color(dfa->total_states());
		stack.push_back(initial_state);
		color.set1(initial_state);
		while (!stack.empty()) {
			state_id_t parent = stack.back(); stack.pop_back();
			bool has_utf8_head = false;
			dfa->for_each_move(parent,
				[&](state_id_t child, auchar_t ch) {
					if ((ch & 0x80) == 0 || (ch & 0xC0) == 0xC0) {
						has_utf8_head = true;
					}
					if (color.is0(child)) {
						color.set1(child);
						stack.push_back(child);
					}
				});
			if (initial_state != parent && has_utf8_head) {
				push_epsilon(initial_state, parent);
			}
		}
		sort_unique_shrink_all();
	}

	void print_nfa_info(FILE* fp, const char* indent_word = "") const {
		assert(NULL != indent_word);
		size_t node_eps = 0, node_non_eps = 0;
		size_t edge_eps = 0, edge_non_eps = 0;
		for(size_t i = 0; i < epsilon.end_i(); ++i) {
			auto&  x = epsilon.val(i);
			node_eps += x.size() ? 1 : 0;
			edge_eps += x.size();
		}
		for(size_t i = 0; i < non_epsilon.end_i(); ++i) {
			auto&  x = non_epsilon.val(i);
			node_non_eps += x.size() ? 1 : 0;
			edge_non_eps += x.size();
		}
		fprintf(fp, "%snumber of states which has ____epsilon: %zd\n", indent_word, node_eps);
		fprintf(fp, "%snumber of states which has non_epsilon: %zd\n", indent_word, node_non_eps);
		fprintf(fp, "%snumber of all ____epsilon transitions: %zd\n", indent_word, edge_eps);
		fprintf(fp, "%snumber of all non_epsilon transitions: %zd\n", indent_word, edge_non_eps);
	}
typedef NFA_BaseOnDFA MyType;
#include "ppi/nfa_algo.hpp"
#include "ppi/nfa_mutation_algo.hpp"
};

template<class StateID>
struct CompactReverseNFA {
	valvec<StateID> index; // parallel with Automata::states
	valvec<CharTarget<StateID> > data;
	gold_hash_map<StateID, valvec<StateID> > epsilon;
	febitvec final_bits;

	enum { sigma = 512 };
	size_t get_sigma() const { return sigma; }

	typedef StateID state_id_t;

	void clear() {
		index.clear();
		data.clear();
		final_bits.clear();
		epsilon.clear();
	}

	void erase_all() {
		index.erase_all();
		data.erase_all();
		final_bits.erase_all();
		epsilon.erase_all();
	}

	void set_final(size_t s) {
		assert(s < index.size()-1);
		if (final_bits.size() <= s) {
			final_bits.resize(s+1);
		}
		final_bits.set1(s);
	}
	bool is_final(size_t s) const {
		assert(s < index.size()-1);
		if (s >= final_bits.size())
			return false;
		else
			return final_bits.is1(s);
	}
	size_t total_states() const { return index.size()-1; }

	/// will not add duplicated targets
	void add_epsilon(StateID source, StateID target) {
		auto& eps = epsilon[source];
		auto  ins_pos = std::lower_bound(eps.begin(), eps.end(), target);
		if (eps.end() == ins_pos || target < *ins_pos) {
		//	fprintf(stderr, "add_epsilon: source=%u target=%u\n", source, target);
			eps.insert(ins_pos, target);
		}
	}

	/// may add duplicated targets
	void push_epsilon(StateID source, StateID target) {
		epsilon[source].push_back(target);
	}

	void sort_unique_shrink_all() {
		for(size_t i = 0; i < epsilon.end_i(); ++i) {
			auto& eps = epsilon.val(i);
			std::sort(eps.begin(), eps.end());
			eps.trim(std::unique(eps.begin(), eps.end()));
			eps.shrink_to_fit();
		}
		epsilon.shrink_to_fit();
		for(size_t i = 0; i < index.size()-1; ++i) {
			size_t lo = index[i+0];
			size_t hi = index[i+1];
			std::sort(data.begin()+lo, data.begin()+hi);
		}
		index.shrink_to_fit();
		data.shrink_to_fit();
	}

	void get_epsilon_targets(StateID s, valvec<StateID>* children) const {
		assert(s < index.size()-1);
		children->erase_all();
		size_t idx = epsilon.find_i(s);
		if (epsilon.end_i() != idx) {
			auto& eps = epsilon.val(idx);
			children->assign(eps.begin(), eps.end());
		//	fprintf(stderr, "get_epsilon_targets(%u):", s);
		//	for (StateID t : eps) fprintf(stderr, " %u", t);
		//	fprintf(stderr, "\n");
		}
	}
	void get_non_epsilon_targets(StateID s, valvec<CharTarget<size_t> >* children) const {
		assert(s < index.size()-1);
		children->erase_all();
		size_t lo = index[s+0];
		size_t hi = index[s+1];
		children->assign(data.begin()+lo, hi-lo);
	}

	void print_nfa_info(FILE* fp, const char* indent_word = "") const {
		assert(NULL != indent_word);
		size_t edge_eps = 0;
		for(size_t i = 0; i < epsilon.end_i(); ++i) {
			edge_eps += epsilon.val(i).size();
		}
		fprintf(fp, "%snumber of states which has epsilon: %zd\n", indent_word, epsilon.size());
		fprintf(fp, "%snumber of all epsilon transitions: %zd\n", indent_word, edge_eps);
	}
typedef CompactReverseNFA MyType;
#include "ppi/nfa_algo.hpp"
};

} // namespace terark
