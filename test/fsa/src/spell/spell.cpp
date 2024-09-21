#define ENABLE_deprecated_match_edit_distance_algo
#include <terark/fsa/deprecated_edit_distance_matcher.hpp>
#include <terark/fsa/dawg.hpp>
#include <terark/fsa/onfly_minimize.hpp>
#include <terark/util/autoclose.hpp>
#include <terark/util/profiling.hpp>
#include <terark/util/linebuf.hpp>
#include <getopt.h>

#if defined(__DARWIN_C_ANSI)
	#define malloc_stats() (void)(0)
#else
	#include <malloc.h>
#endif

using namespace terark;

template<class State>
class MyDAWG : public DAWG<State> {
	typedef DAWG<State> super;
	typedef MyDAWG MyType;
public:
#include <terark/fsa/ppi/dfa_using_template_base.hpp>
#include <terark/fsa/ppi/dfa_deprecated_edit_distance_algo.hpp>
#include <terark/fsa/ppi/accept.hpp>
#include <terark/fsa/ppi/adfa_minimize.hpp>
#include <terark/fsa/ppi/dfa_hopcroft.hpp>
#include <terark/fsa/ppi/match_path.hpp>
};


void usage(const char* prog) {
	fprintf(stderr, "usage: %s -p pattern_file -f scaning_file [-s] [-b state_size]\n", prog);
	exit(3);
}

int loop_cnt = 10;
const char* pattern_file = NULL;
const char* scaning_file = NULL;
size_t max_edit_distance = 1;

std::string str_rev(const std::string& str) {
	std::string rev = str;
	std::reverse(rev.begin(), rev.end());
	return rev;
}

template<class Au, class Onfly>
int run0() {
#if 1
	using ::tolower;
#else // don't use tolower, use identity
	#define tolower [](unsigned char c) { return c; }
#endif
	printf("%s\n", BOOST_CURRENT_FUNCTION);
	int lineno = 0;
	terark::profiling pf;
	long long t0, t1, t2, t3, t4, t5;
	size_t bytes = 0;
	size_t pattern_bytes_all  = 0;
	size_t pattern_bytes_uniq = 0;
	size_t pattern_num_all  = 0;
	size_t pattern_num_uniq = 0;
	terark::LineBuf line;
	Au au1, au2; {
		Onfly onfly1(&au1), onfly2(&au2);
		terark::Auto_fclose fp(fopen(pattern_file, "r"));
		if (NULL == fp) {
			fprintf(stderr, "failed: fopen(%s, r) = %s\n", pattern_file, strerror(errno));
			return 1;
		}
		t0 = pf.now();
		while ((line.getline(fp)) > 0) {
			line.chomp();
			lineno++;
			if (0 == line.n) continue;
			std::transform(line.begin(), line.end(), line.begin(), tolower);
			if (onfly1.add_word(line)) {
			//	printf("add_word: %s\n", line.p);
				std::reverse(line.begin(), line.end());
				bool b2 = onfly2.add_word(line);
				assert(b2); TERARK_UNUSED_VAR(b2);
				pattern_num_uniq++, pattern_bytes_uniq += line.n;
			}
			pattern_num_all++;
			pattern_bytes_all += line.n;
		}
//		malloc_stats();
	}
	t1 = pf.now();

// When used for match_edit_distance, must not path_zip
//	au1.path_zip("DFS");
//	au2.path_zip("DFS");

	au1.compile();
	au2.compile();
	t2 = pf.now();
	printf("pattern_num[all=%zd uniq=%zd] pattern_bytes[all=%zd uniq=%zd] mem_size=%zd\n"
			, pattern_num_all, pattern_num_all
			, pattern_bytes_all, pattern_bytes_uniq
			, au1.mem_size());
	valvec<std::string> queries;
	if (scaning_file) {
		if (strcmp(scaning_file, "+") == 0)
			scaning_file = pattern_file;
		terark::Auto_fclose fp(fopen(scaning_file, "r"));
		if (NULL == fp) {
			fprintf(stderr, "failed: fopen(%s, r) = %s\n", scaning_file, strerror(errno));
			goto Done;
		}
		lineno = 0;
		while (line.getline(fp) > 0) {
			lineno++;
			line.chomp();
			if (0 == line.n) continue;
			std::transform(line.begin(), line.end(), line.begin(), tolower);
			queries.emplace_back(line.p, line.n);
		}

		t3 = pf.now();
		EditDistanceMatcher matched1(max_edit_distance);
	//	size_t threshold = 1000*1000;
		size_t threshold = 10;
	  if (au1.num_words() < threshold) {
		// search whole query by match_edit_distance
		for (size_t i = 0; i < queries.size(); ++i) {
			size_t n_try = 0;
			std::string word(queries[i]);
			for (int i = 0; i < loop_cnt; ++i) {
				n_try = au1.match_edit_distance(word, matched1);
			}
			printf("slow_search_try_num=%zd  input: %s\n", n_try, word.c_str());
			for (size_t i = 0; i < matched1.size(); ++i) {
				for (size_t j = 0; j < matched1[i].end_i(); ++j) {
					fstring  candidate = matched1[i].key(j);
					unsigned match_pathes = matched1[i].val(j);
					size_t   lex_index = au1.index(candidate);
					printf("\tedit_distance=%zd(%u): %s  lex_index=%zd\n", i, match_pathes, candidate.p, lex_index);
				}
			}
			bytes += word.size();
		}
		// End: search whole query by match_edit_distance
	  }

		t4 = pf.now();
		// search exact-prefix + fuzzy-suffix
		EditDistanceMatcher matched2(max_edit_distance);
		valvec<typename Au::state_id_t> path;
		for (size_t i = 0; i < queries.size(); ++i) {
			using namespace std;
			const std::string rev = str_rev(queries[i]);
			fstring word(queries[i]);
			fstring prefix(word.p, word.n/2);
			fstring rev_prefix(rev.data(), rev.size()/2);
			size_t n_try1 = 0, n_try2 = 0;
			for (int lc = 0; lc < loop_cnt; ++lc) {
				au1.match_path(prefix, &path, tolower);
				if (path.size() < 2) {
					fprintf(stderr, "forward search: not found: %s\n", word.p);
					goto Skip;
				}
				std::string suffix = std::string(prefix.end(), word.end());
				n_try1 = au1.match_edit_distance(path.back(), suffix, matched1);
			}
			for (int lc = 0; lc < loop_cnt; ++lc) {
				au2.match_path(rev_prefix, &path, tolower);
				if (path.size() < 2) {
					fprintf(stderr, "backward search: not found: %s\n", word.p);
					goto Skip;
				}
				std::string suffix = std::string(rev_prefix.end(), rev.data() + rev.size());
				n_try2 = au2.match_edit_distance(path.back(), suffix, matched2);
			}
			printf("fast_search_try_num=%zd  input: %s\n", n_try1 + n_try2, word.p);
			for (size_t j = 0; j < matched1.size(); ++j) {
				for (size_t k = 0; k < matched1[j].end_i(); ++k) {
					std::string candidate = prefix.str();
					fstring suffix = matched1[j].key(k);
					candidate.append(suffix.p, suffix.n);
					size_t lex_index = au1.index(candidate);
					unsigned match_pathes = matched1[j].val(k);
					printf("\t_forward_edit_distance=%zd(%u): %s  lex_index=%zd\n", j, match_pathes, candidate.c_str(), lex_index);
				}
			}
			for (size_t j = 0; j < matched2.size(); ++j) {
				for (size_t k = 0; k < matched2[j].end_i(); ++k) {
					std::string candidate = rev_prefix.str();
					fstring suffix = matched2[j].key(k);
					candidate.append(suffix.p, suffix.n);
					std::reverse(candidate.begin(), candidate.end());
					size_t lex_index = au1.index(candidate);
					unsigned match_pathes = matched2[j].val(k);
					printf("\tbackward_edit_distance=%zd(%u): %s  lex_index=%zd\n", j, match_pathes, candidate.c_str(), lex_index);
				}
			}
		}
	Skip:;
		// End: search exact-prefix + fuzzy-suffix
	}
	else {
Done:
		t3 = t4 = t2;
	}
	t5 = pf.now();
	printf("read+insert[time=%f's speed=%fMB/s]\n", pf.sf(t0,t1), double(pattern_bytes_all)/pf.uf(t0,t1));
	printf("compile[time=%f's speed=%fMB/s]\n", pf.sf(t1,t2), double(pattern_bytes_all)/pf.uf(t1,t2));
	printf("slow match_edit_distance[time=%f's speed=%fMB/s query/sec=%f]\n", pf.sf(t3,t4)
			, double(loop_cnt * bytes )/pf.uf(t3,t4)
			, double(loop_cnt * queries.size())/pf.sf(t3,t4)
			);
	printf("fast match_edit_distance[time=%f's speed=%fMB/s query/sec=%f]\n", pf.sf(t4,t5)
			, double(loop_cnt * bytes )/pf.uf(t4,t5)
			, double(loop_cnt * queries.size())/pf.sf(t4,t5)
			);
	printf("states=%zd transitions=%zd mem_size=%zd\n", au1.total_states(), au1.total_transitions(), au1.mem_size());
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
	int state_bytes = 6;
	int is_sorted  = 0;
	for (;;) {
		int opt = getopt(argc, argv, "d:f:l:p:b:s");
		switch (opt) {
		default:
			usage(argv[0]);
			break;
		case -1:
			goto GetoptDone;
		case '?':
			fprintf(stderr, "invalid option: %c\n", optopt);
			return 3;
		case 'd':
			max_edit_distance = atoi(optarg);
			break;
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
	case 0x04: ret = run<MyDAWG<State4B>, 0>(); break;
	case 0x05: ret = run<MyDAWG<State5B>, 0>(); break;
	case 0x06: ret = run<MyDAWG<State6B>, 0>(); break;
	case 0x08: ret = run<MyDAWG<State32>, 0>(); break;
	case 0x14: ret = run<MyDAWG<State4B>, 1>(); break;
	case 0x15: ret = run<MyDAWG<State5B>, 1>(); break;
	case 0x16: ret = run<MyDAWG<State6B>, 1>(); break;
	case 0x18: ret = run<MyDAWG<State32>, 1>(); break;
	}
	return ret;
}

