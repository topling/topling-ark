#pragma once

#include <terark/fsa/dense_dfa.hpp>
#include <terark/zsrch/zsrch.hpp>

namespace terark { namespace zsrch {

class RegexQuery : public Query {
public:
	std::unique_ptr<DenseDFA_uint32_320> m_qryDFA;
	RegexQuery(fstring strQuery);
	~RegexQuery();
};

}} // namespace terark::zsrch

