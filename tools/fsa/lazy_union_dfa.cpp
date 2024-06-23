#define _CRT_SECURE_NO_WARNINGS
#define _SCL_SECURE_NO_WARNINGS

#include <terark/fsa/dawg.hpp>
#include <terark/fsa/fsa_for_union_dfa.hpp>
#include <terark/util/profiling.hpp>
#include <terark/util/autoclose.hpp>
#include <terark/util/linebuf.hpp>
#include <getopt.h>

#if defined(__DARWIN_C_ANSI)
	#define malloc_stats() (void)(0)
#else
	#include <malloc.h>
#endif

#ifdef _MSC_VER
	#define strcasecmp _stricmp
	#define malloc_stats() (void)(0)
	typedef intptr_t ssize_t;
#endif

using namespace terark;

void usage(const char* prog) {
	fprintf(stderr, "usage: %s [-b state_size] -O big_fast_dfa | -o small_slow_dfa input_dfa_file(s)\n", prog);
	exit(3);
}

terark::profiling pf;

const char* outfile = NULL;
bool save_as_mmap = false;

int main(int argc, char* argv[]) {
#if TERARK_WORD_BITS >= 64
	const char* unziped_txt_file = NULL;
	const char* dfa_file_list_fname = NULL;
	size_t bench_loop = 1;
	bool check = false;
	for (;;) {
		int opt = getopt(argc, argv, "O:mu:cb:f:");
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
			bench_loop = atoi(optarg);
			break;
		case 'c':
			check = true;
			break;
		case 'f':
			dfa_file_list_fname = optarg;
			break;
		case 'O':
			outfile = optarg;
			break;
		case 'm':
			save_as_mmap = true;
			break;
		case 'u':
			unziped_txt_file = optarg;
			break;
		}
	}
GetoptDone:
	valvec<std::unique_ptr<MatchingDFA> > dfavec;
	size_t allsub_states = 0;
	auto load_one_dfa = [&](const char* fname) {
		MatchingDFA* dfa = MatchingDFA::load_from(fname);
		size_t states = dfa->v_total_states();
		allsub_states += states;
		if (states >= UINT32_MAX) {
			fprintf(stderr
				, "ERROR: sub dfa is too large: fname: \"%s\", states = %zd\n"
				, fname, states);
			exit(1);
		}
		dfavec.emplace_back(dfa);
	};
	if (dfa_file_list_fname) {
		Auto_fclose fp;
		if (strcmp(dfa_file_list_fname, "-") != 0) {
			fp = fopen(dfa_file_list_fname, "r");
			if (!fp) {
				fprintf(stderr, "ERROR: fopen(%s) = %s\n"
					, dfa_file_list_fname, strerror(errno));
				return 1;
			}
		}
		LineBuf line;
		while (line.getline(fp.self_or(stdin)) > 0) {
			line.chomp();
			load_one_dfa(line.p);
		}
	}
	else {
		if (optind == argc) {
			fprintf(stderr, "missing input dfa files\n");
			usage(argv[0]);
		}
		for (int i = optind; i < argc; ++i) {
			const char* fname = argv[i];
			load_one_dfa(fname);
		}
	}
#if defined(NDEBUG)
	try {
#endif
	fprintf(stderr, "subdfa_cnt=%zd lazyDFA.allsub_states=%zd\n",
			dfavec.size(), allsub_states);
	long long t0 = pf.now();
	auto pDfaVec = (const MatchingDFA**)dfavec.data();
	bool takeOwn = true;
	std::unique_ptr<MatchingDFA>
		lazyDFA(createLazyUnionDFA(pDfaVec, dfavec.size(), takeOwn));
	dfavec.risk_set_size(0); // release ownership of dfa objects
	long long t1 = pf.now();
	fprintf(stderr, "createLazyUnionDFA: time = %8.3f sec\n", pf.sf(t0, t1));
	if (unziped_txt_file) {
		lazyDFA->print_output(unziped_txt_file);
	}
	auto dawg = lazyDFA->get_dawg();
	if (bench_loop && !dawg) {
		fprintf(stderr, "lazyUnionDFA is not a dawg or nlt, bench will not run!\n");
	}
	size_t wordlenSum = 0;
	if (check && dawg) {
		std::string word;
		long long t0 = pf.now();
		for(size_t i = 0; i < dawg->num_words(); ++i) {
			dawg->nth_word(i, &word);
			size_t j = dawg->index(word);
			if (j != i) {
				fprintf(stderr
					, "dawg check fail: i = %zd, j = %zd, word = %s\n"
					, i, j, word.c_str()
				);
			}
			wordlenSum += word.size();
		}
		long long t1 = pf.now();
		fprintf(stderr
			, "DAWG Check, op=(word+index): %9.3f sec, QPS = %9.3fK, us/op: %9.3f\n"
			, pf.sf(t0,t1)
			, dawg->num_words()/pf.mf(t0,t1)
			, pf.uf(t0,t1)/dawg->num_words()
		);
	}
	if (check) {
		long long t0 = pf.now();
		lazyDFA->for_each_word([&](size_t nth, fstring word) {
			if (!lazyDFA->exact_match(word)) {
				fprintf(stderr
					, "exact_match fail: nth = %zd, word = %.*s\n"
					, nth, word.ilen(), word.data()
				);
			}
		});
		long long t1 = pf.now();
		fprintf(stderr
			, "DFA  Check, op=(walk+match): %9.3f sec, QPS = %9.3fK, us/op: %9.3f\n"
			, pf.sf(t0,t1)
			, dawg->num_words()/pf.mf(t0,t1)
			, pf.uf(t0,t1)/dawg->num_words()
		);
	}
	fstrvecl words;
	if (dawg) {
		words.reserve(dawg->num_words());
		words.reserve_strpool(wordlenSum);
	}
//	long long matchTime = 0;
	if (bench_loop && dawg) {
		long long t0 = pf.now();
		for (size_t loop = 0; loop < bench_loop; ++loop) {
			std::string word;
			for (size_t i = 0; i < dawg->num_words(); ++i) {
				dawg->nth_word(i, &word);
				if (0 == loop)
					words.push_back(word);
			}
		}
		long long t1 = pf.now();
		fprintf(stderr
			, "DAWG Bench, op=(index only): %9.3f sec, QPS = %9.3fK, us/op: %9.3f, ThroughPut = %9.3fMB/s\n"
			, pf.sf(t0,t1)
			, bench_loop*dawg->num_words()/pf.mf(t0,t1)
			, pf.uf(t0,t1)/dawg->num_words()/bench_loop
			, bench_loop*words.strpool.size()/pf.uf(t0,t1)
		);
		t0 = pf.now();
		for (size_t loop = 0; loop < bench_loop; ++loop) {
			for (size_t i = 0; i < dawg->num_words(); ++i) {
				dawg->index(words[i]);
			}
		}
		t1 = pf.now();
		fprintf(stderr
			, "DAWG Bench, op=(fetch only): %9.3f sec, QPS = %9.3fK, us/op: %9.3f, ThroughPut = %9.3fMB/s\n"
			, pf.sf(t0,t1)
			, bench_loop*dawg->num_words()/pf.mf(t0,t1)
			, pf.uf(t0,t1)/dawg->num_words()/bench_loop
			, bench_loop*words.strpool.size()/pf.uf(t0,t1)
		);
		t0 = pf.now();
		for (size_t loop = 0; loop < bench_loop; ++loop) {
			for (size_t i = 0; i < dawg->num_words(); ++i) {
				fstring word = words[i];
				if (!lazyDFA->exact_match(word)) {
					fprintf(stderr, "exact_match failed: word = %.*s\n", word.ilen(), word.data());
				}
			}
		}
		t1 = pf.now();
		fprintf(stderr
			, "DFA  Bench, op=(match only): %9.3f sec, QPS = %9.3fK, us/op: %9.3f, ThroughPut = %9.3fMB/s\n"
			, pf.sf(t0,t1)
			, bench_loop*dawg->num_words()/pf.mf(t0,t1)
			, pf.uf(t0,t1)/dawg->num_words()/bench_loop
			, bench_loop*words.strpool.size()/pf.uf(t0,t1)
		);
	}
	return 0;
#if defined(NDEBUG)
	}
	catch (const std::exception& ex) {
		fprintf(stderr, "failed: caught exception: %s\n", ex.what());
		return 1;
	}
#endif

#else
	fprintf(stderr, "%s must be compiled on 64 bit platform\n", argv[0]);
	return 1;
#endif
}

