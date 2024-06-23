#include <terark/fsa/dawg.hpp>
#include <terark/util/linebuf.hpp>
#include <terark/util/profiling.hpp>

using namespace terark;

int main(int argc, char* argv[]) {
	Automata<State32> au1, au2, au3, au4;
	terark::profiling pf;
	terark::LineBuf line;
	while (line.getline(stdin) > 0) {
		line.chomp();
		au1.add_word(line);
	}
	long t1 = pf.now();
	au1.adfa_minimize(au2);
	long t2 = pf.now();
//	au1.trie_dfa_minimize(au3);
	au1.graph_dfa_minimize(au3);
	long t3 = pf.now();
	au1.adfa_minimize2(au4);
	long t4 = pf.now();
	printf("states: trie=%zd adfa_minimize1=%zd graph_dfa_minimize=%zd\n"
		, au1.total_states()
		, au2.total_states()
		, au3.total_states()
		);
	printf("states: trie=%zd adfa_minimize2=%zd graph_dfa_minimize=%zd\n"
		, au1.total_states()
		, au4.total_states()
		, au3.total_states()
		);
	printf("time_msec: adfa_minimize1=%f graph_dfa_minimize=%f graph/adfa=%f adfa/graph=%f\n"
		, pf.mf(t1,t2)
		, pf.mf(t2,t3)
		, pf.mf(t2,t3) / pf.mf(t1,t2)
		, pf.mf(t1,t2) / pf.mf(t2,t3)
		);
	printf("time_msec: adfa_minimize2=%f graph_dfa_minimize=%f graph/adfa=%f adfa/graph=%f\n"
		, pf.mf(t3,t4)
		, pf.mf(t2,t3)
		, pf.mf(t2,t3) / pf.mf(t3,t4)
		, pf.mf(t3,t4) / pf.mf(t2,t3)
		);
	au2.set_is_dag(true);
	au3.set_is_dag(true);
	au4.set_is_dag(true);
//	au2.write_dot_file("adfa_minimize.dot");
//	au3.write_dot_file("graph_dfa_minimize.dot");
	au2.print_output("adfa_minimize1.txt");
	au3.print_output("graph_dfa_minimize.txt");
	au4.print_output("adfa_minimize2.txt");
	return 0;
}

