#include "automata.hpp"
#include "tmplinst.hpp"
#include "dfa_mmap_header.hpp"

namespace terark {

TMPL_INST_DFA_CLASS(Automata<State32>)
TMPL_INST_DFA_CLASS(Automata<State4B>)
TMPL_INST_DFA_CLASS(Automata<State5B>)
TMPL_INST_DFA_CLASS(Automata<State4B_448>)

TMPL_INST_DFA_CLASS(Automata<State6B>)
TMPL_INST_DFA_CLASS(Automata<State6B_512>)

#if !defined(_MSC_VER)

//TMPL_INST_DFA_CLASS(Automata<State32_512>) // inst in mre_tmplinst.cpp
TMPL_INST_DFA_CLASS(Automata<State5B_448>)

#if TERARK_WORD_BITS >= 64 && defined(TERARK_INST_ALL_DFA_TYPES)
TMPL_INST_DFA_CLASS(Automata<State34>)
TMPL_INST_DFA_CLASS(Automata<State38>)
TMPL_INST_DFA_CLASS(Automata<State7B>)
TMPL_INST_DFA_CLASS(Automata<State7B_448>)
#endif
#endif

// Unexpected: HOPCROFT_USE_HASH=true is slower, let default be false
static bool g_use_hash = getEnvBool("HOPCROFT_USE_HASH", false);
bool HopcroftUseHash() { return g_use_hash; }

} // namespace terark
