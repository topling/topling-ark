#include "dfa_algo.hpp"

namespace terark {

/*
NonRecursiveForEachWord::
NonRecursiveForEachWord(const NonRecursiveForEachWord&) = default;

NonRecursiveForEachWord&
NonRecursiveForEachWord::operator=(const NonRecursiveForEachWord&) = default;
*/

NonRecursiveForEachWord*
NonRecursiveForEachWord::clone() const {
//	return new NonRecursiveForEachWord(*this);
	return NULL;
}

NonRecursiveForEachWord::NonRecursiveForEachWord(MatchContext* ctx) {
	m_ctx = ctx;
	m_word_buf.reserve(64);
	m_stack.reserve(1*1024);
}

NonRecursiveForEachWord::~NonRecursiveForEachWord() {
}

} // namespace terark

