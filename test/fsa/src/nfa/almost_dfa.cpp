#include <terark/fsa/automata.hpp>
#include <terark/fsa/dawg.hpp>
#include <terark/fsa/onfly_minimize.hpp>
#include <terark/util/linebuf.hpp>
#include <terark/util/profiling.hpp>

using namespace terark;

int main() {
	terark::profiling pf;
	typedef Automata<State32> DFA;
	DFA dfa1, dfa2;
	fstrvec lines;
	{
		terark::LineBuf line;
		while (line.getline(stdin) > 0) {
			line.chomp();
			lines.emplace_back(line.p, line.n);
		}
	}
	{
		long t0 = pf.now();
		DFA::state_id_t roots[2];
		for (int i = 0; i < 2; ++i) {
			roots[i] = dfa1.new_state();
			MinADFA_OnflyBuilder<DFA> builder(&dfa1, roots[i]);
			for (size_t j = 0; j < lines.size(); ++j)
				builder.add_word(lines[j]);
		}
		DFA dfa3;
		NFA_BaseOnDFA<DFA> nfa(&dfa1);
		nfa.concat(roots[0], roots[1]);
		nfa.add_epsilon(initial_state, roots[0]);
		valvec<unsigned> *power_index = NULL, power_set;
		nfa.make_dfa(power_index, power_set, &dfa3);
		dfa3.swap(dfa1);
		long t1 = pf.now();
		printf("time=%f'ms dfa1.states=%zd power_set.size=%zd\n"
			 , pf.mf(t0, t1), dfa1.total_states(), power_set.size());
	}
	{
		long t0 = pf.now();
		MinADFA_OnflyBuilder<DFA> builder(&dfa2);
		std::string word;
		for (size_t i = 0; i < lines.size(); ++i) {
			fstring line1 = lines[i];
			word.assign(line1.p, line1.n);
			for (size_t j = 0; j < lines.size(); ++j) {
				fstring line2 = lines[j];
				word.append(line2.p, line2.n);
				builder.add_word(word);
				word.resize(line1.size());
			}
		//	builder.add_word(line1);
		}
		long t1 = pf.now();
		printf("time=%f'ms dfa2.states=%zd\n", pf.mf(t0, t1), dfa2.num_used_states());
	}
	dfa1.print_output("concat_dfa1.txt");
	dfa2.print_output("concat_dfa2.txt");
	dfa1.path_zip("DFS");
	dfa1.write_dot_file("concat_dfa1.dot");
	return 0;
}

