#define _CRT_SECURE_NO_WARNINGS
#define _SCL_SECURE_NO_WARNINGS

#include <terark/fsa/dawg.hpp>
#include <terark/fsa/linear_dfa.hpp>
#include <terark/fsa/dense_dfa.hpp>
#include <terark/fsa/fsa_for_union_dfa.hpp>
#include <terark/util/profiling.hpp>
#include <terark/util/autoclose.hpp>
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
	fprintf(stderr, "Usage: %s Options Input-DFA-Files...\n"
		"Description:\n"
		"    Union all Input-DFA-Files into one DFA\n"
		"Options:\n"
		"    -O Large-DFA-File, large, but pretty fast\n"
		"    -o Small-DFA-File, small, but pretty slow, now deprecated\n"
		"    -M No-Argument, Minimize the output DFA\n"
		"    -m [Save-As-MemMap], argment is optional, default is 1\n"
		"       Note: If not specified this option, use default(1)\n"
		"             If the arguement is 0, it means \"DO NOT Save-As-MemMap\"\n"
		"    -u Union-Method, could be 1, 2, 3, default is 2\n"
		"       1: use dfa_lazy_union_n\n"
		"       2: use dfa_union_n\n"
		"       3: use NFA_ForMultiRootDFA to built a virtual nfa and use nfa-to-dfa\n"
		"          conversion to built the unioned dfa\n"
		"    -2 No-Argument, use adfa_minimize2\n"
		"       adfa_minimize2 use a customized hash table for equivalent-state register\n"
		, prog);
	exit(3);
}

terark::profiling pf;

const char* outfile = NULL;
const char* ldfa_outfile = NULL;
bool do_minimize = true;
bool save_as_mmap = true;
bool use_min2 = false;
int union_method = 2;
int ldfa_state_bytes = 0;
int state_bytes = 32; // not 32 bytes, 32 indicate State32_512
int g_minZpathLen = 2;
unsigned max_sigma = 256;

template<class Ldfa, class Au>
void compress_to_linear(const Au& au) {
	printf("%s\n", BOOST_CURRENT_FUNCTION);
	long long t1 = pf.now();
	Ldfa ldfa;
	ldfa.build_from("PFS", au);
	ldfa.set_is_dag(true);
	long long t2 = pf.now();
	printf("linear_dfa: states=%zd mem_size=%zd time=%f's\n", ldfa.total_states(), ldfa.mem_size(), pf.sf(t1,t2));
	if (save_as_mmap) {
		printf("writing (small) linear_dfa file: %s", ldfa_outfile);
	   	fflush(stdout);
		ldfa.save_to(ldfa_outfile);
	} else {
		printf("writing (small) linear_dfa mmap file: %s", ldfa_outfile);
	   	fflush(stdout);
		ldfa.save_mmap(ldfa_outfile);
	}
	long long t3 = pf.now();
	printf(" : time=%f's\n", pf.sf(t2,t3));
}

template<template<int,int> class Ldfa, class Au, int Sigma>
void ldfa_dispatch(const Au& au) {
	switch (ldfa_state_bytes) {
		default:
			fprintf(stderr, "invalid ldfa_state_bytes=%d\n", ldfa_state_bytes);
			break;
	#ifdef TERARK_INST_ALL_DFA_TYPES
		case 2: compress_to_linear<Ldfa<2, Sigma> >(au); break;
		case 3: compress_to_linear<Ldfa<3, Sigma> >(au); break;
	#endif
		case 4: compress_to_linear<Ldfa<4, Sigma> >(au); break;
#if !defined(_MSC_VER)
		case 5: compress_to_linear<Ldfa<5, Sigma> >(au); break;
	#if TERARK_WORD_BITS >= 64
		case 6: compress_to_linear<Ldfa<6, Sigma> >(au); break;
	#endif
	#if TERARK_WORD_BITS >= 64 && defined(TERARK_INST_ALL_DFA_TYPES)
		case 7: compress_to_linear<Ldfa<7, Sigma> >(au); break;
		case 8: compress_to_linear<Ldfa<8, Sigma> >(au); break;
	#endif
#endif
	}
}

typedef Automata<State32_512> WorkingDFA;
int write_fast_dfa(WorkingDFA& au);

// mDFA also has an NFA facade
int run(FSA_ForUnionDFA& mDFA) {
	printf("%s\n", BOOST_CURRENT_FUNCTION);
	long long t0, t1, t2;
	bool is_dag = mDFA.is_dag();
	printf("mDFA.states=%zd is_dag=%d\n", mDFA.total_states(), is_dag);
	t0 = pf.now();
	WorkingDFA au;
	WorkingDFA tailmin; // multi-root tail minimized DFA
	size_t RootForPathZip = initial_state;
	valvec<size_t> roots, roots2;
	mDFA.get_all_root(&roots);
	if (is_dag) {
		if (use_min2)
			mDFA.adfa_minimize2(roots, tailmin, &roots2);
		else
			mDFA.adfa_minimize(roots, tailmin, &roots2);
	} else {
		mDFA.hopcroft_multi_root_dfa(roots, tailmin, &roots2);
	}
	mDFA.clear(); // free memory
	t1 = pf.now();
	printf("mDFA: %s_minimize time=%f's\n", is_dag ? "adfa" : "hopcroft", pf.sf(t0, t1));
	printf("mDFA: tailmin: roots=%zd states=%zd \n", roots2.size(), tailmin.total_states());

	std::sort(roots2.begin(), roots2.end());
	roots2.trim(std::unique(roots2.begin(), roots2.end()));
	t0 = pf.now();
	if (2 == union_method) {
		printf("dfa_lazy_union_n...");
	   	fflush(stdout);
		RootForPathZip = tailmin.dfa_lazy_union_n(roots2);
		au.swap(tailmin);
	}
	else if (1 == union_method) {
		printf("dfa_union_n...");
	   	fflush(stdout);
		au.dfa_union_n(tailmin, roots2);
	}
	else {
		printf("converting nfa of tailmin-dfa to union-dfa...");
	   	fflush(stdout);
		NFA_ForMultiRootDFA<WorkingDFA> nfa(&tailmin);
		for (auto root : roots2) nfa.add_root(root);
		nfa.make_dfa(&au);
	}
	t1 = pf.now();
	printf(" time=%f's\n", pf.sf(t0,t1));
	printf("OutputDFA[mem_size=%ld states=%ld transitions=%ld]\n"
			, (long)au.mem_size()
			, (long)au.total_states()
			, (long)au.total_transitions()
			);
	size_t words = 0;
	size_t sumlen = au.dag_total_path_len(&words);
	t2 = pf.now();
	printf("is_dag=1 allwords=%zd sumlen=%zd time=%f's\n", words, sumlen, pf.sf(t1, t2));
	if (g_minZpathLen) {
		t1 = pf.now();
		tailmin.path_zip(&RootForPathZip, 1, au, "DFS", g_minZpathLen);
		t2 = pf.now();
		printf("path_zip: states=%zd mem_size=%zd time=%f's\n"
				, au.total_states(), au.mem_size(), pf.sf(t1,t2));
	}
	au.set_is_dag(is_dag);
	if (ldfa_outfile) {
		ldfa_dispatch<LinearDFA, WorkingDFA, (WorkingDFA::sigma>256)?512:256>(au);
	}
	if (outfile) {
		write_fast_dfa(au);
	}
	malloc_stats();
	return 0;
}

int main(int argc, char* argv[]) {
	for (;;) {
		int opt = getopt(argc, argv, "b:o:O:l:mMz:2u::");
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
			ldfa_outfile = optarg;
			break;
		case 'O':
			outfile = optarg;
			break;
		case 'l':
			ldfa_state_bytes = atoi(optarg);
			break;
		case 'm':
			if (optarg && atoi(optarg) == 0)
				save_as_mmap = false;
			break;
		case 'M':
			do_minimize = false;
			break;
		case 'z':
			g_minZpathLen = atoi(optarg);
			g_minZpathLen = std::max(g_minZpathLen, 0);
			break;
		case '2':
			use_min2 = true;
			break;
		case 'u':
			if (optarg) {
			   	union_method = atoi(optarg);
				switch (union_method) {
				default:
					fprintf(stderr, "\t-u union_method must be 0 1 2\n");
					return 3;
				case 0: case 1: case 2: break; // OK
				}
			}
			break;
		}
	}
GetoptDone:
	if (NULL == outfile && NULL == ldfa_outfile) {
		fprintf(stderr, "-O fast_dfa_file or -o small_dfa_file is required\n");
		return 3;
	}
	if (ldfa_state_bytes <= 0) {
		ldfa_state_bytes = state_bytes;
	}
	else if (ldfa_state_bytes > state_bytes) {
		fprintf(stderr, "-l ldfa_state_bytes is too big, set to state_bytes:%d\n", state_bytes);
		ldfa_state_bytes = state_bytes;
	}
	if (optind == argc) {
		fprintf(stderr, "missing input dfa files\n");
		usage(argv[0]);
	}
	FSA_ForUnionDFA nfa;
	nfa.set_own(true);
	for (int i = optind; i < argc; ++i) {
		const char* fname = argv[i];
		const BaseDFA* dfa = BaseDFA::load_from(fname);
		if (dfa->num_zpath_states()) {
			fprintf(stderr, "error: dfa must not be path zipped!\n"
					" file: '%s' num_zpath_states=%zd\n"
					, fname, dfa->num_zpath_states());
			return 1;
		}
		max_sigma = std::max<unsigned>(max_sigma, dfa->get_sigma());
		nfa.add_dfa(dfa);
	}
	nfa.compile();
	return run(nfa);
}

int write_fast_dfa(WorkingDFA& au) {
	try {
		int ret = 0;
		switch (state_bytes) {
		default:
			fprintf(stderr, "invalid state_bytes=%d\n", state_bytes);
			return 3;
#define OutputDFA(DFA1, DFA2) \
			if (max_sigma > DFA2::sigma) { \
				fprintf(stderr, "fatal: max_sigma=%u DFA1=(%s) DFA2=(%s)\n", max_sigma, #DFA1, #DFA2); \
				return 1; \
			} \
			if (max_sigma <= 256) { \
				DFA1 dst; \
			   	dst.copy_with_pzip(au); \
				dst.save_to(outfile); \
			} else { \
				DFA2 dst; \
			   	dst.copy_with_pzip(au); \
				dst.save_to(outfile); \
			} \
		   	break;
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#ifdef _MSC_VER
	#define OutputDFA_256(DFA1, DFA2) \
			if (max_sigma <= 256) { \
				DFA1 dst; \
			   	dst.copy_with_pzip(au); \
				dst.save_to(outfile); \
			} else { \
				fprintf(stderr, "fatal: max_sigma=%u DFA1=(%s)\n", max_sigma, #DFA1); \
				return 1; \
			} \
		   	break;
#else
	#define OutputDFA_256 OutputDFA
#endif
		case 0x04: OutputDFA(Automata<State4B>, Automata<State4B_448>);
		case 0x05: OutputDFA_256(Automata<State5B>, Automata<State5B_448>);
		case 0x06: OutputDFA(Automata<State6B>, Automata<State6B_512>);
		case   32: OutputDFA(Automata<State32>, Automata<State32_512>); // sizeof(State32) == 8
	#if TERARK_WORD_BITS >= 64 && !defined(_MSC_VER) && defined(TERARK_INST_ALL_DFA_TYPES)
		case   34: OutputDFA(Automata<State34>, Automata<State32_512>); // sizeof(State34) == 8
	#endif
		case   96: OutputDFA(DenseDFA<uint32_t>, DenseDFA_uint32_320 ); // sizeof(DenseDFA::State) == (80 or 96)
		}
		return ret;
	}
	catch (const std::exception& ex) {
		fprintf(stderr, "failed: caught exception: %s\n", ex.what());
		return 1;
	}
}

