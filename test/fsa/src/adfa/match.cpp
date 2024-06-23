#include <terark/fsa/dawg.hpp>
#include <terark/fsa/onfly_minimize.hpp>
#include <terark/util/linebuf.hpp>
#include <terark/util/autoclose.hpp>
#include <terark/util/profiling.hpp>
#include <getopt.h>

#if defined(__DARWIN_C_ANSI)
	#define malloc_stats() (void)(0)
#else
	#include <malloc.h>
#endif

using namespace terark;

void usage(const char* prog) {
	fprintf(stderr, "usage: %s -p pattern_file -f scaning_file [-s] [-b state_size]\n", prog);
	exit(3);
}

int loop_cnt = 10;
const char* pattern_file = NULL;
const char* scaning_file = NULL;

template<class Au, class Onfly>
int run0() {
	printf("%s\n", BOOST_CURRENT_FUNCTION);
	terark::LineBuf line;
	int lineno = 0;
	terark::profiling pf;
	long long t0, t1, t2, t3, t4;
	size_t bytes = 0;
	size_t pattern_bytes_all  = 0;
	size_t pattern_bytes_uniq = 0;
	size_t pattern_num_all  = 0;
	size_t pattern_num_uniq = 0;
	Au au; {
		Onfly onfly(&au);
		terark::Auto_fclose fp(fopen(pattern_file, "r"));
		if (NULL == fp) {
			fprintf(stderr, "failed: fopen(%s, r) = %s\n", pattern_file, strerror(errno));
			return 1;
		}
		t0 = pf.now();
		while (line.getline(fp) >= 0) {
			lineno++;
			line.chomp();
			if (line.size() == 0) continue;
			if (onfly.add_word(line))
				pattern_num_uniq++, pattern_bytes_uniq += line.size();
			pattern_num_all++;
			pattern_bytes_all += line.size();
		}
//		malloc_stats();
	}
	t1 = pf.now();
	au.path_zip("DFS");
	t2 = pf.now();
	printf("pattern_num[all=%zd uniq=%zd] pattern_bytes[all=%zd uniq=%zd] mem_size=%zd\n"
			, pattern_num_all, pattern_num_all
			, pattern_bytes_all, pattern_bytes_uniq
			, au.mem_size());
	t3 = pf.now();
	long num_matched = 0;
	long num_non_empty = 0;
	if (scaning_file) {
		if (strcmp(scaning_file, "+") == 0)
			scaning_file = pattern_file;
		terark::Auto_fclose fp(fopen(scaning_file, "r"));
		if (NULL == fp) {
			fprintf(stderr, "failed: fopen(%s, r) = %s\n", scaning_file, strerror(errno));
			goto Done;
		}
		lineno = 0;
		while (line.getline(fp) >= 0) {
			lineno++;
			line.chomp();
			if (line.size() == 0) continue;
			for (int i = 0; i < loop_cnt; ++i)
			//	if (au.accept(line)) num_matched++;
				au.tpl_match_prefix(line, [&](size_t){num_matched++;});
			bytes += line.size();
			num_non_empty++;
		}
		num_matched /= loop_cnt;
	}
Done:
	t4 = pf.now();
	printf("read+insert[time=%f's speed=%fMB/s]\n", pf.sf(t0,t1), double(pattern_bytes_all)/pf.uf(t0,t1));
	printf("matchprefix[time=%f's speed=%fMB/s]\n", pf.sf(t3,t4), double(loop_cnt * bytes )/pf.uf(t3,t4));
	printf("path_zip.time=%f's\n", pf.sf(t1,t2));
	printf("states=%zd transitions=%zd mem=%zd\n", au.total_states(), au.total_transitions(), au.mem_size());
	printf("matched=%ld\n", num_matched);
//	printf("matched=%ld unmatched=%ld\n", num_matched, num_non_empty-num_matched);
	malloc_stats();
	return 0;
}

template<class Au, int is_sorted>
int run() {
	try {
		return run0<Au
			, typename boost::mpl::if_c
				< is_sorted
				, typename MinADFA_OnflyBuilder<Au>::Ordered
				, MinADFA_OnflyBuilder<Au>
				>::type
			>();
	}
	catch (const std::exception& ex) {
		fprintf(stderr, "caught exception.what= %s\n", ex.what());
		return 1;
	}
}

int main(int argc, char* argv[]) {
	int state_bytes = 8;
	int is_sorted  = 0;
	for (;;) {
		int opt = getopt(argc, argv, "f:l:p:b:s");
		switch (opt) {
		default:
			usage(argv[0]);
			break;
		case -1:
			goto GetoptDone;
		case '?':
			fprintf(stderr, "invalid option: %c\n", optopt);
			return 3;
		case 'l':
			loop_cnt = atoi(optarg);
			break;
		case 'p':
			pattern_file = optarg;
			break;
		case 'b':
			state_bytes = atoi(optarg);
			break;
		case 's':
			is_sorted = 1;
			break;
		case 'f':
			scaning_file = optarg;
			break;
		}
	}
GetoptDone:
	if (NULL == pattern_file) {
		fprintf(stderr, "option -p pattern_file is required\n");
		return 3;
	}
	int ret = 0;
	switch (is_sorted * 16 + state_bytes) {
	default:
		fprintf(stderr, "invalid state_bytes=%d\n", state_bytes);
		return 3;
	case 0x04: ret = run<Automata<State4B>, 0>(); break;
	case 0x05: ret = run<Automata<State5B>, 0>(); break;
	case 0x06: ret = run<Automata<State6B>, 0>(); break;
	case 0x08: ret = run<Automata<State32>, 0>(); break;
	case 0x14: ret = run<Automata<State4B>, 1>(); break;
	case 0x15: ret = run<Automata<State5B>, 1>(); break;
	case 0x16: ret = run<Automata<State6B>, 1>(); break;
	case 0x18: ret = run<Automata<State32>, 1>(); break;
	}
	return ret;
}

