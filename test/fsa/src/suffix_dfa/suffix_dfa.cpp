#include <terark/fsa/suffix_dfa.hpp>
#include <terark/fsa/dawg.hpp>
#include <terark/fsa/onfly_minimize.hpp>
#include <terark/util/profiling.hpp>
#include <terark/util/linebuf.hpp>

using namespace terark;

typedef Automata<State32> DFA;
terark::profiling pf;

int main(int argc, char* argv[]) {
	DFA dfa1, dfa2, dfa3, dfa4;
	long t0 = pf.now(), t1, t2, t3, t4, t5, t6;
   	if (argc >= 2 && strcmp(argv[1], "-whole") == 0) {
		terark::LineBuf line;
		std::string str;
		while (line.getline(stdin) > 0) {
			line.chomp();
			str.append(line.p, line.n);
		}
		t1 = pf.now();
		dfa1.add_word(str);
	}
	else {
		valvec<std::string> lines;
		terark::LineBuf line;
		while (line.getline(stdin) > 0) {
			line.chomp();
			lines.emplace_back(line.p, line.n);
		}
		t1 = pf.now();
		MinADFA_OnflyBuilder<DFA> builder(&dfa1);
		for (fstring line : lines)
			builder.add_word(line);
	}
	t2 = pf.now();
	dfa2 = dfa1;
	t3 = pf.now();
	CreateSuffixDFA(dfa2);
	t4 = pf.now();
	dfa2.graph_dfa_minimize(dfa3);
	t5 = pf.now();
	dfa4.path_zip(dfa3, "DFS");
	t6 = pf.now();
	printf( "time_msec[read=%f add=%f copy=%f CreateSuffixDFA=%f minimize=%f path_zip=%f]\n"
		  , pf.mf(t0, t1)
		  , pf.mf(t1, t2)
		  , pf.mf(t2, t3)
		  , pf.mf(t3, t4)
		  , pf.mf(t4, t5)
		  , pf.mf(t5, t6)
		  );
	dfa2.print_output("/dev/stdout");
	return 0;
}

