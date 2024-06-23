#define _SCL_SECURE_NO_WARNINGS
#include <terark/fsa/fsa.hpp>
#include <terark/util/linebuf.hpp>
using namespace terark;
void OnMatch(int klen, int, fstring value) {
	printf("%d\t%.*s\n", klen, value.ilen(), value.data());
}
int main(int argc, char* argv[]) {
	if (argc < 2) {
		fprintf(stderr, "usage: %s dfa_file\n", argv[0]);
		return 1;
	}
	std::unique_ptr<MatchingDFA> dfa(MatchingDFA::load_from(argv[1]));
	terark::LineBuf line;
	while (line.getline(stdin) > 0) {
		line.chomp();
		dfa->match_key_l('\t', fstring(line.p, line.n), &OnMatch, &tolower);
	}
	return 0;
}

