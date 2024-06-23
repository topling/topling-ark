#include <terark/fsa/dawg.hpp>
#include <terark/fsa/onfly_minimize.hpp>
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

void usage(const char* prog) {
	fprintf(stderr, "usage: %s [-b state_size]\n", prog);
	exit(3);
}

const char* outfile = NULL;

template<class Au>
int run() {
	printf("%s\n", BOOST_CURRENT_FUNCTION);
	 size_t len1 = 0;
	ssize_t len2 = 0;
	char*   line = NULL;
	long lineno = 0;
	terark::profiling pf;
	long long t0, t1, t2;
	size_t pattern_bytes_all  = 0;
	size_t pattern_bytes_uniq = 0;
	size_t pattern_num_all  = 0;
	size_t pattern_num_uniq = 0;
	Au au; {
		MinADFA_OnflyBuilder<Au> onfly(&au);
		t0 = pf.now();
		while ((len2 = getline(&line, &len1, stdin)) >= 0) {
			lineno++;
			while (len2 && isspace(line[len2-1])) --len2;
			if (0 == len2) continue;
			line[len2] = 0;
			if (onfly.add_word(fstring(line, len2)))
				pattern_num_uniq++, pattern_bytes_uniq += len2;
			if (lineno % 100000 == 0) {
				printf("lineno=%ld, DFA[mem_size=%ld states=%ld transitions=%ld]\n"
						, lineno
						, (long)au.mem_size()
						, (long)au.total_states()
						, (long)au.total_transitions()
						);
			}
			pattern_num_all++;
			pattern_bytes_all += len2;
		}
	}
	size_t non_path_zip_size = au.mem_size();
	t1 = pf.now();
	au.path_zip("DFS");
	t2 = pf.now();
	printf("pattern_num[all=%zd uniq=%zd] pattern_bytes[all=%zd uniq=%zd] mem_size=%zd non_path_zip_size=%zd\n"
			, pattern_num_all, pattern_num_all
			, pattern_bytes_all, pattern_bytes_uniq
			, au.mem_size()
			, non_path_zip_size
			);
	printf("time: read+insert=%f's path_zip=%f\n", pf.sf(t0,t1), pf.sf(t1,t2));
	{
		using namespace terark;
		FileStream file(outfile, "wb");
		file.disbuf();
		NativeDataOutput<OutputBuffer> dio; dio.attach(&file);
		dio << au;
	}
	malloc_stats();
	return 0;
}

int main(int argc, char* argv[]) {
	int state_bytes = 8;
	for (;;) {
		int opt = getopt(argc, argv, "b:o:");
		switch (opt) {
		default:
			usage(argv[0]);
			break;
		case -1:
			goto GetoptDone;
		case '?':
			fprintf(stderr, "invalid option: %c\n", optopt);
			return 3;
		case 'b':
			state_bytes = atoi(optarg);
			break;
		case 'o':
			outfile = optarg;
			break;
		}
	}
GetoptDone:
	if (NULL == outfile) {
		fprintf(stderr, "-o file is required\n");
		return 3;
	}
	int ret = 0;
	switch (state_bytes) {
	default:
		fprintf(stderr, "invalid state_bytes=%d\n", state_bytes);
		return 3;
	case 0x04: ret = run<Automata<State4B> >(); break;
	case 0x05: ret = run<Automata<State5B> >(); break;
	case 0x06: ret = run<Automata<State6B> >(); break;
	case 0x08: ret = run<Automata<State32> >(); break;
	}
	return ret;
}

