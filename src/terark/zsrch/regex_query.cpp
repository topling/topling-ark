#include "regex_query.hpp"
#include <terark/fsa/create_regex_dfa.hpp>

namespace terark { namespace zsrch {

RegexQuery::RegexQuery(fstring strQuery) {
	BaseDFA* dfa = create_regex_dfa(strQuery, "");
	m_qryDFA.reset(dynamic_cast<DenseDFA_uint32_320*>(dfa));
	assert(m_qryDFA.get() != NULL);
}

RegexQuery::~RegexQuery() {
	//
}

}} // namespace terark::zsrch

