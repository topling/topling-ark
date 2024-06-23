#pragma once

#include <terark/fsa/fsa.hpp>

namespace terark {

TERARK_DLL_EXPORT
BaseDFA* create_regex_dfa(fstring regex, fstring regex_option);

} // namespace terark
