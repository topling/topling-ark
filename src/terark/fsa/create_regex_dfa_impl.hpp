#ifndef __terark_fsa_create_regex_dfa_impl_hpp__
#define __terark_fsa_create_regex_dfa_impl_hpp__

#include "re2/vm_nfa.hpp"
#include "dense_dfa.hpp"
#include "nfa.hpp"
#include <re2/regexp.h>
#include <re2/prog.h>

namespace terark {

TERARK_DLL_EXPORT
BaseDFA* create_regex_dfa(fstring regex, fstring regex_option) {
	using re2::Regexp;
	re2::RegexpStatus err;
	re2::StringPiece strRegex(regex.data(), regex.ilen());
//	fprintf(stderr, "parsing regex: %.*s\n", regex.ilen(), regex.data());
	int flags = Regexp::LikePerl;
	if (regex_option.strstr("m")) flags |= Regexp::MatchNL;
	if (regex_option.strstr("i")) flags |= Regexp::FoldCase;
	Regexp* re = Regexp::Parse(strRegex, Regexp::ParseFlags(flags), &err);
	if (re == NULL) {
		THROW_STD(invalid_argument, "re2 Parse: %.*s  ERROR:%s\n"
			, regex.ilen(), regex.data(), err.Text().c_str());
	}
	size_t max_mem = 128 * 1024 * 1024; // 128M
//	fprintf(stderr, "CompileToProg\n");
	std::unique_ptr<re2::Prog> prog(re->CompileToProg(max_mem));
	if (!prog) {
		re->Decref();
		THROW_STD(invalid_argument,
			"re2 Compile: %.*s", regex.ilen(), regex.data());
	}
	re->Decref();
	RE2_VM_NFA nfa(&*prog);
	nfa.m_exclude_char = -1;
	std::unique_ptr<DenseDFA_uint32_320> dfa(new DenseDFA_uint32_320);
//	fprintf(stderr, "make_dfa...\n");
	nfa.make_dfa(&*dfa);
	dfa->graph_dfa_minimize();
	dfa->set_is_dag(dfa->tpl_is_dag());

	return dfa.release();
}

} // namespace terark

#endif // __terark_fsa_create_regex_dfa_impl_hpp__

