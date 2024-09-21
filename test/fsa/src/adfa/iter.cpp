#include <terark/fsa/dfa_algo.hpp>
#include <terark/fsa/automata.hpp>
#include <getopt.h>

using namespace terark;

int main(int argc, char* argv[]) {
	std::auto_ptr<MatchingDFA> dfa(MatchingDFA::load_from(stdin));
	ADFA_LexIteratorUP iter(dfa->adfa_make_iter());
	iter->seek_lower_bound(argc >= 2 ? argv[1] : "");
	while (iter->incr()) {
		fstring word = iter->word();
		printf("%.*s\n", word.ilen(), word.data());
	}
	return 0;
}

