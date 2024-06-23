#include <terark/fsa/nfa.hpp>
#include <terark/fsa/automata.hpp>
#include <terark/util/linebuf.hpp>
#include <terark/util/autoclose.hpp>

using namespace terark;

typedef GenericNFA<NFA_Transition4B> nfa_t;

void test0() {
	nfa_t nfa;
	size_t s1 = nfa.new_state();
	size_t s2 = nfa.new_state();
	nfa.add_move(0, 0, 'a');
	nfa.add_move(0, 0, 'b');
	nfa.add_move(0, 0, 'c');
	nfa.add_move(0, 0, 'd');
	nfa.add_move(0, s1, 'a');
//	nfa.add_move(0, s1, 'b');
	nfa.add_epsilon(0, s1);
	nfa.add_move(s1, s2, 'a');
	nfa.add_move(s1, s2, 'b');
	nfa.set_final(s2);
	nfa.write_dot_file("nfa.dot");
	Automata<State6B> dfa;
	nfa.make_dfa(&dfa);
	dfa.write_dot_file("dfa.dot");
	fprintf(stderr, "Successed\n");
}

int main(int argc, char* argv[]) {
	terark::LineBuf line;
	nfa_t nfa;
	Automata<State32> dfa1, dfa2;
	while (line.getline(stdin) > 0) {
		line.chomp();
		nfa.add_word(line);
		dfa1.add_word(line);
	}
	nfa.make_dfa(&dfa2);
	dfa1.print_output("dfa1.txt");
	dfa2.print_output("dfa2.txt");
	dfa1.write_dot_file("dfa1.dot");
	dfa2.write_dot_file("dfa2.dot");
	nfa .write_dot_file("nfa2.dot");
	return 0;
}

