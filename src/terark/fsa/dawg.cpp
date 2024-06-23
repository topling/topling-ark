#include "dawg.hpp"
#include "tmplinst.hpp"

namespace terark {

TMPL_INST_DFA_CLASS(DAWG<State32>)
TMPL_INST_DFA_CLASS(DAWG<State4B>)
TMPL_INST_DFA_CLASS(DAWG<State5B>)

#if !defined(_MSC_VER)
	TMPL_INST_DFA_CLASS(DAWG<State6B>)

	#if TERARK_WORD_BITS >= 64 && defined(TERARK_INST_ALL_DFA_TYPES)
	TMPL_INST_DFA_CLASS(DAWG<State34>)
	//TMPL_INST_DFA_CLASS(DAWG<State38>)
	TMPL_INST_DFA_CLASS(DAWG<State7B>)
	#endif
#endif

} // namespace terark
