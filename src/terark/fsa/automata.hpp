#pragma once

#include "automata_basic.hpp"
#include "nfa.hpp"
#include "hopcroft.hpp"
#include "onfly_minimize.hpp"
#include <terark/util/unicode_iterator.hpp>

namespace terark {

template<class State>
class Automata : public AutomataTpl<State, MatchingDFA> {
	typedef AutomataTpl<State, MatchingDFA> super;
	typedef Automata MyType;
public:
#include "ppi/dfa_using_template_base.hpp"
#include "ppi/for_each_suffix.hpp"
#include "ppi/match_key.hpp"
#include "ppi/match_path.hpp"
#include "ppi/match_prefix.hpp"
#include "ppi/accept.hpp"
#include "ppi/dfa_reverse.hpp"
#include "ppi/dfa_hopcroft.hpp"
#include "ppi/dfa_const_virtuals.hpp"
#include "ppi/dfa_const_methods_use_walk.hpp"
#include "ppi/adfa_minimize.hpp"
#include "ppi/path_zip.hpp"
#include "ppi/dfa_mutation_virtuals_basic.hpp"
#include "ppi/dfa_mutation_virtuals_extra.hpp"
#include "ppi/dfa_methods_calling_swap.hpp"
#include "ppi/dfa_set_op.hpp"
#include "ppi/state_move_fast.hpp"
#include "ppi/make_edit_distance_dfa.hpp"
#include "ppi/adfa_iter.hpp"
#include "ppi/for_each_word.hpp"
#include "ppi/match_pinyin.hpp"
#include "ppi/post_order.hpp"
};

template<class State>
class AutomataAsAcyclicPathDFA : public AutomataTpl<State, AcyclicPathDFA> {
	typedef AutomataTpl<State, AcyclicPathDFA> super;
	typedef AutomataAsAcyclicPathDFA MyType;
public:
#include "ppi/dfa_using_template_base.hpp"
#include "ppi/dfa_const_virtuals.hpp"
#include "ppi/dfa_const_methods_use_walk.hpp"
#include "ppi/adfa_minimize.hpp"
#include "ppi/path_zip.hpp"
#include "ppi/dfa_mutation_virtuals_basic.hpp"
#include "ppi/dfa_mutation_virtuals_extra.hpp"
#include "ppi/dfa_methods_calling_swap.hpp"
#include "ppi/dfa_set_op.hpp"
#include "ppi/state_move_fast.hpp"
#include "ppi/adfa_iter.hpp"
#include "ppi/for_each_word.hpp"
#include "ppi/post_order.hpp"
};

} // namespace terark
