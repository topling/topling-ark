#if defined(__GNUC__) && __GNUC__ * 1000 + __GNUC_MINOR__ >= 8000
//src/terark/fsa/linear_dawg.hpp:225:10: warning: '<anonymous>' may be used uninitialized in this function [-Wmaybe-uninitialized]
    #pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif

#include "linear_dawg.hpp"
#include "tmplinst.hpp"

namespace terark {

#if defined(TERARK_INST_ALL_DFA_TYPES)
TMPL_INST_DFA_CLASS(LinearDAWG<2>)
TMPL_INST_DFA_CLASS(LinearDAWG<3>)
#endif
TMPL_INST_DFA_CLASS(LinearDAWG<4>)
TMPL_INST_DFA_CLASS(LinearDAWG<5>)

typedef LinearDAWG<4, 16> LinearDAWG_4B_16;
typedef LinearDAWG<4, 64> LinearDAWG_4B_64;

TMPL_INST_DFA_CLASS(LinearDAWG_4B_16)
TMPL_INST_DFA_CLASS(LinearDAWG_4B_64)

#if !defined(_MSC_VER) && 0
typedef LinearDAWG<5, 64> LinearDAWG_5B_64;
TMPL_INST_DFA_CLASS(LinearDAWG_5B_64)
#endif

#if TERARK_WORD_BITS >= 64 && !defined(_MSC_VER)
TMPL_INST_DFA_CLASS(LinearDAWG<6>)
#endif

} // namespace terark

