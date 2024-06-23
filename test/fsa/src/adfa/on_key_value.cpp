#include <terark/fsa/dawg.hpp>
#include <terark/util/profiling.hpp>
#include <terark/io/DataIO.hpp>
#include <terark/io/FileStream.hpp>
#include <terark/io/StreamBuffer.hpp>
#include <getopt.h>

#if defined(__DARWIN_C_ANSI)
	#define malloc_stats() (void)(0)
#else
	#include <malloc.h>
#endif

using namespace terark;

template<class State, class DataIO>
void FindValueByKey(DataIO& dio, uint64_t version, char** words, size_t n) {
	Automata<State> au;
	au.load_au(dio, version);
	for (size_t i = 0; i < n; ++i) {
		const char*  word = words[i];
		const size_t wlen = strlen(word);
		au.match_key('\t', fstring(word, wlen),
		[&](int klen, int idx, fstring value) {
			printf("%.*s [%d] %s\n", klen, word, idx, value.c_str());
		}, &tolower);
	}
}

int main(int argc, char* argv[]) {
	int ret = 0;
	using namespace terark;
	FileStream file; file.attach(stdin);
	NativeDataInput<InputBuffer> dio; dio.attach(&file);
	var_uint64_t version;
	std::string state_name;
	dio >> version;
	if (version.t > 5) {
		goto DFA_is_wrapped;
	}
	dio >> state_name;
#define ON_STATE(State) \
	else if (typeid(State).name() == state_name) { \
		fprintf(stderr, "state_name: %-30s : %s\n", state_name.c_str(), #State); \
		FindValueByKey<State>(dio, version.t, argv+1, argc-1); \
		file.detach(); \
	}
//-------------------------
	if (0) {}
	ON_STATE(State4B)
	ON_STATE(State5B)
	ON_STATE(State6B)
	ON_STATE(State32)
	ON_STATE(DAWG_State<State4B>)
	ON_STATE(DAWG_State<State5B>)
	ON_STATE(DAWG_State<State6B>)
	ON_STATE(DAWG_State<State32>)
#if TERARK_WORD_BITS >= 64 && defined(TERARK_INST_ALL_DFA_TYPES)
	ON_STATE(State7B)
	ON_STATE(DAWG_State<State7B>)
#endif
	else {
		DFA_is_wrapped:
		try {
			std::string className;
			dio.risk_set_bufpos(0);
			dio >> className;
			dio.risk_set_bufpos(0);
			fprintf(stderr, "use MatchingDFA::load_from, DFA_class=%s\n", className.c_str());
			std::unique_ptr<BaseDFA> dfa(BaseDFA::load_from_NativeInputBuffer(&dio));
			if (!dfa) {
				fprintf(stderr, "MatchingDFA::load_from(stdin) failed\n");
				ret = 3;
				goto Done;
			}
			MatchingDFA* mdfa = dynamic_cast<MatchingDFA*>(dfa.get());
			if (!dfa) {
				fprintf(stderr, "DFA className=%s is not a MatchingDFA\n", className.c_str());
				ret = 3;
				goto Done;
			}
			for (int i = 1; i < argc; ++i) {
				printf("---------------------------\n");
				const char*  word = argv[i];
				const size_t wlen = strlen(word);
				int plen = mdfa->match_key('\t', fstring(word, wlen),
					[&](int klen, int idx, fstring value) {
						printf("%.*s [%d] %s\n", klen, word, idx, value.c_str());
					}, &tolower);
				printf("plen=%02d %s\n", plen, word);
			}
		}
		catch (const std::exception& ex) {
			fprintf(stderr, "exception: %s\n", ex.what());
		}
	}
Done:
	file.detach();
	return ret;
}

