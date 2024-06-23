#include <terark/fsa/automata.hpp>
#include <terark/fsa/dawg.hpp>
#include <terark/fsa/onfly_minimize.hpp>
#include <terark/fsa/string_as_dfa.hpp>
#include <terark/util/linebuf.hpp>
#include <terark/util/profiling.hpp>

using namespace terark;

typedef Automata<State32> DFA;
terark::profiling pf;

template<class DFA1>
void run(long t0, DFA1& dfa1) {
	printf("%s\n", BOOST_CURRENT_FUNCTION);
	DFA dfa2, dfa3, dfa4;
	long t1 = pf.now();
	NFA_BaseOnDFA<DFA1> nfa(&dfa1);
	DFS_GraphWalker<typename DFA1::state_id_t> walker;
	walker.resize(dfa1.total_states());
	walker.putRoot(initial_state);
	while (!walker.is_finished()) {
		auto curr = walker.next();
		if (curr != initial_state && !dfa1.is_term(curr))
			nfa.push_epsilon(initial_state, curr);
		walker.putChildren(&dfa1, curr);
	}
	long t2 = pf.now();
	size_t power_set_size;
	{
		valvec<unsigned> *power_idx = NULL, power_set;
		nfa.make_dfa(power_idx, power_set, &dfa2);
		power_set_size = power_set.size();
	}
	long t3 = pf.now();
	dfa2.graph_dfa_minimize(dfa3);
	long t4 = pf.now();
	dfa4.path_zip(dfa3, "DFS");
	long t5 = pf.now();
	printf("states[dfa1=%zd dfa2=%zd dfa3=%zd dfa4=%zd] power_set.size=%zd\n"
		 , dfa1.total_states()
		 , dfa2.total_states()
		 , dfa3.total_states()
		 , dfa4.total_states()
		 , power_set_size
		 );
	printf("time_msec[dfa1=%f build_nfa=%f nfa_to_dfa=%f dfa_minimize=%f path_zip=%f]\n"
		 , pf.mf(t0, t1)
		 , pf.mf(t1, t2)
		 , pf.mf(t2, t3)
		 , pf.mf(t3, t4)
		 , pf.mf(t4, t5)
		 );
	dfa1.print_output("orignal_dfa.txt");
	if (dfa4.dag_pathes() <= 100)
		dfa4.print_output("/dev/stdout");
	dfa3.write_dot_file("suffix_dfa.dot");
}

int main(int argc, char* argv[]) {
	DFA dfa2, dfa3, dfa4;
   	if (argc >= 2 && strcmp(argv[1], "-whole") == 0) {
		StringAsDFA dfa1;
		terark::LineBuf line;
		long t0 = pf.now();
		while (line.getline(stdin) > 0) {
			line.chomp();
			dfa1.str.append(line.p, line.n);
		}
		run(t0, dfa1);
	}
	else {
		DFA dfa1;
		valvec<std::string> lines;
		terark::LineBuf line;
		while (line.getline(stdin) > 0) {
			line.chomp();
			lines.emplace_back(line.p, line.n);
		}
		long t0 = pf.now();
		MinADFA_OnflyBuilder<DFA> builder(&dfa1);
		for (fstring line : lines)
			builder.add_word(line);
		run(t0, dfa1);
	}
	return 0;
}

