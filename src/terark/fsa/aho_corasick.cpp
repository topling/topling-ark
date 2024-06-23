#include "aho_corasick.hpp"
#include "tmplinst.hpp"

namespace terark {

TMPL_INST_DFA_CLASS(DoubleArrayTrie_8B)
TMPL_INST_DFA_CLASS(Aho_Corasick_DoubleArray_8B)
TMPL_INST_DFA_CLASS(Aho_Corasick_SmartDFA_AC_State32)
#if !defined(_MSC_VER)
TMPL_INST_DFA_CLASS(Aho_Corasick_SmartDFA_AC_State5B)
#endif
TMPL_INST_DFA_CLASS(Aho_Corasick_SmartDFA_AC_State4B)
//TMPL_INST_DFA_CLASS(Aho_Corasick_SmartDFA_AC_State6B)

TMPL_INST_DFA_CLASS(AC_FullDFA<uint32_t>)
TMPL_INST_DFA_CLASS(FullDFA<uint32_t>)

} // namespace terark
