#ifndef __terark_automata_re2_nfa_adapter_hpp__
#define __terark_automata_re2_nfa_adapter_hpp__

#include <terark/fsa/fsa.hpp>
#include <terark/fsa/power_set.hpp>
#include <terark/bitmap.hpp>
#include <terark/util/autoclose.hpp>
#include <terark/util/fstrvec.hpp>
#include <terark/util/profiling.hpp>
#include <terark/util/throw.hpp>
#include <boost/noncopyable.hpp>

namespace re2 {

	class Prog; // forward declaration

}

namespace terark {

	class TERARK_DLL_EXPORT RE2_VM_NFA : boost::noncopyable {
	private:
	//	const re2::Prog* m_prog;
		re2::Prog* m_prog;
		basic_fstrvec<auchar_t> m_captures;
		valvec<int> m_cap_ptrs;

	public:
		enum { sigma = 258 };
		typedef uint32_t state_id_t;

		explicit RE2_VM_NFA(re2::Prog* prog);
		~RE2_VM_NFA();

		std::string m_val_prefix;
		bool  m_add_dot_star;
		bool  m_use_bin_val;
		bool  m_do_capture;
		int   m_exclude_char;

		size_t get_sigma() const { return sigma; }

		void compile();

		size_t captures() const { return m_captures.size(); }
		size_t total_states() const;
		bool is_final(size_t) const;
		void get_epsilon_targets(state_id_t s, valvec<state_id_t>* children) const;
		void get_non_epsilon_targets(state_id_t s, valvec<CharTarget<size_t> >* children) const;

		typedef RE2_VM_NFA MyType;
		#include "../ppi/nfa_algo.hpp"
	};

	TERARK_DLL_EXPORT
	BaseDFA* create_regex_dfa(fstring regex, fstring regex_option);

} // namespace terark

#endif // __terark_automata_re2_nfa_adapter_hpp__

