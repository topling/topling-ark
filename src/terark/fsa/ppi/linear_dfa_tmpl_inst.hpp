
#ifndef LargeSigma
	#error LargeSigma must be defined
#endif

#ifndef MyType
	#error MyType must be defined
#endif

#include "../tmplinst.hpp"

namespace terark {

#if defined(TERARK_INST_ALL_DFA_TYPES)
TMPL_INST_DFA_CLASS(MyType<2>)
TMPL_INST_DFA_CLASS(MyType<3>)
#endif
TMPL_INST_DFA_CLASS(MyType<4>)
TMPL_INST_DFA_CLASS(MyType<5>)

#if defined(TERARK_INST_ALL_DFA_TYPES)
TMPL_INST_DFA_CLASS(BOOST_PP_CAT(MyType, _4B_16))
TMPL_INST_DFA_CLASS(BOOST_PP_CAT(MyType, _4B_32))
TMPL_INST_DFA_CLASS(BOOST_PP_CAT(MyType, _5B_32))
#endif

#if !defined(_MSC_VER)

#if defined(TERARK_INST_ALL_DFA_TYPES)
TMPL_INST_DFA_CLASS(TERARK_PP_CAT3(MyType, _2B_, LargeSigma))
TMPL_INST_DFA_CLASS(TERARK_PP_CAT3(MyType, _3B_, LargeSigma))
#endif
TMPL_INST_DFA_CLASS(TERARK_PP_CAT3(MyType, _4B_, LargeSigma))
TMPL_INST_DFA_CLASS(TERARK_PP_CAT3(MyType, _5B_, LargeSigma))

#if TERARK_WORD_BITS >= 64
	TMPL_INST_DFA_CLASS(MyType<6>)
	TMPL_INST_DFA_CLASS(TERARK_PP_CAT3(MyType, _6B_, LargeSigma))
#endif

#endif // _MSC_VER

} // namespace terark

#undef MyType
#undef LargeSigma

