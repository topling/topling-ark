#define _CRT_SECURE_NO_WARNINGS
#define _SCL_SECURE_NO_WARNINGS

#include <terark/fsa/automata.hpp>
#include <terark/fsa/linear_dfa.hpp>
#include <terark/fsa/louds_dfa.hpp>
#include <terark/fsa/quick_dfa.hpp>
#include <terark/fsa/squick_dfa.hpp>
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

#define DFA_Template Automata

template<class StateXX>
struct MyDFA : DFA_Template<StateXX> {
	typedef DFA_Template<StateXX> PathZipDFA1;
	typedef DFA_Template<StateXX> PathZipDFA2;
};
#if TERARK_WORD_BITS >= 64 && defined(TERARK_INST_ALL_DFA_TYPES)
#include <terark/fsa/dfa_with_register.hpp>
template<>
struct MyDFA<State38> : DFA_WithOnflyRegister<DFA_Template<State38> > {
	typedef DFA_Template<State34> PathZipDFA1;
	typedef DFA_Template<State34> PathZipDFA2;
//	typedef DFA_Template<State7B> PathZipDFA2;
};
#endif

typedef MyDFA<State32> MainDFA;
typedef MainDFA::PathZipDFA1 MainPzDFA;

void usage(const char* prog) {
	fprintf(stderr, "usage: %s [-b state_size]\n", prog);
	exit(3);
}

terark::profiling pf;

const char* finput_name = NULL;
const char* xdfa_file = NULL;
const char* ldfa_file = NULL;
const char* qdfa_file = NULL;
const char* sdfa_file = NULL;
const char* udfa_file = NULL;
const char* udfa_file_il = NULL;
const char* ldfa_walk_method = "PFS";
bool write_dot = false;
bool write_text = false;
bool check_ldfa = false;
bool save_as_mmap = true;
bool warn_dup = false;
bool be_quiet = false;
NestLoudsTrieConfig g_zp_conf;
int state_int_name = -1;
int is_input_sorted = 0;
int g_minZpathLen = 2;

const char* mapred_job_id; // check for if running in hadoop map reduce

#if defined(_MSC_VER)
std::string demangled_name(const char* name) { return name; }
#else
#include <cxxabi.h>
//__cxa_demangle(const char* __mangled_name, char* __output_buffer, size_t* __length, int* __status);
std::string demangled_name(const char* name) {
	int status = 0;
	terark::AutoFree<char> realname(
		abi::__cxa_demangle(name, NULL, NULL, &status)
	);
	return std::string(realname.p);
}
#endif

template<class Ldfa, class Au>
void build_ldfa_from_au(const char* WalkMethod, Ldfa& ldfa, const Au& au) {
	ldfa.build_from(ldfa_walk_method, au);
}
template<class RankSelect, class Au>
void build_ldfa_from_au(const char* /*WalkMethod*/,
						GenLoudsDFA<RankSelect>& ldfa, const Au& au) {
	ldfa.build_from(au, g_zp_conf);
}

template<class Ldfa, class Au>
void do_save_linear_dfa(const Au& au, const char* foutput) {
	if (!be_quiet)
		fprintf(stderr, "%s\n", BOOST_CURRENT_FUNCTION);
	std::string ldfa_class = demangled_name(typeid(Ldfa).name());
	const char* ldc = ldfa_class.c_str();
	long long t1 = pf.now();
	Ldfa ldfa;
	build_ldfa_from_au(ldfa_walk_method, ldfa, au);
	ldfa.set_is_dag(true);
	long long t2 = pf.now();
	if (!be_quiet)
		fprintf(stderr, "%s: states=%zd mem_size=%zd time=%f's\n", ldc, ldfa.total_states(), ldfa.mem_size(), pf.sf(t1,t2));
#if defined(WITH_CHECK_LINEAR)
	if (check_ldfa) {
		fprintf(stderr, "checking %s...\n", ldc);
		std::string fdfa_class = demangled_name(typeid(Au).name());
		const char* fdc = fdfa_class.c_str();
		long long t3;
		long long cnt = 0;
		const int loop = 10;
		t1 = pf.now();
		au.tpl_for_each_word([&](size_t nth, fstring w, size_t) {
				bool exists;
				for (int i = 0; i < loop; ++i)
					exists = ldfa.accept(w);
				if (!exists) {
					fprintf(stderr, "nth=%zd accept=false: %.*s\n", nth, w.ilen(), w.p);
				}
				cnt++;
			});
		t2 = pf.now();
		au.tpl_for_each_word([&](size_t nth, fstring w, size_t) {
				bool exists;
				for (int i = 0; i < loop; ++i)
					exists = au.accept(w);
				if (!exists) {
					fprintf(stderr, "nth=%zd accept=false: %.*s\n", nth, w.ilen(), w.p);
				}
			});
		t3 = pf.now();
		fprintf(stderr, "time: [%s]=%8.3f [%s]=%8.3f, speedup=%f\n", ldc, pf.sf(t1,t2), fdc, pf.sf(t2, t3), pf.sf(t1,t2)/pf.sf(t2, t3));
		fprintf(stderr, "mQPS: [%s]=%8.3f [%s]=%8.3f\n", ldc, cnt*loop/pf.uf(t1,t2), fdc, cnt*loop/pf.uf(t2, t3));
		fprintf(stderr, "ns/Q: [%s]=%8.3f [%s]=%8.3f\n", ldc, pf.nf(t1,t2)/loop/cnt, fdc, pf.nf(t2, t3)/cnt/loop);
	}
#endif
	if (save_as_mmap) {
		if (!be_quiet)
			fprintf(stderr, "write (small) [%s] mmap file: %s\n", ldc, foutput);
		ldfa.save_mmap(foutput);
	} else {
		if (!be_quiet)
			fprintf(stderr, "write (small) [%s] norm file: %s\n", ldc, foutput);
		ldfa.save_to(foutput);
	}
	if (write_text) {
		au.print_output("output.txt.au");
		ldfa.print_output("output.txt.ldfa");
	}
}

template<bool IsInputSorted>
int build(MainPzDFA& pz) {
	if (!be_quiet)
		fprintf(stderr, "%s\n", BOOST_CURRENT_FUNCTION);
	long long t0, t1, t2;
	size_t lineno = 0;
	size_t record_bytes_all  = 0;
	size_t record_bytes_uniq = 0;
	size_t record_num_all  = 0;
	size_t record_num_uniq = 0;
	MainDFA au; {
		terark::Auto_fclose finput;
		if (finput_name) {
			finput = fopen(finput_name, "r");
			if (NULL == finput) {
				fprintf(stderr, "failed: fopen(%s, r) = %s\n", finput_name, strerror(errno));
				return 3;
			}
		} else {
			if (!be_quiet)
				fprintf(stderr, "reading from stdin\n");
		}
		t0 = pf.now();
		terark::LineBuf line;
		while (line.getline(finput.self_or(stdin)) >= 0) {
			lineno++;
			line.chomp();
			if (0 == line.n) continue;
			if (au.add_word(line))
				record_num_uniq++, record_bytes_uniq += line.n;
			else if (warn_dup)
				fprintf(stderr, "lineno=%zd, dup: %s\n", lineno, line.p);

			if (!be_quiet && lineno % 100000 == 0) {
				fprintf(stderr, "lineno=%zd, DFA[mem_size=%zd states=%zd transitions=%zd]\n"
						, lineno
						, au.mem_size()
						, au.total_states()
						, au.total_transitions()
						);
			}
			record_num_all++;
			record_bytes_all += line.n;
		}
	}
	if (!be_quiet)
		fprintf(stderr, "lineno=%zd, DFA[mem_size=%zd states=%zd transitions=%zd]\n"
			, lineno
			, au.mem_size()
			, au.total_states()
			, au.total_transitions()
			);
	size_t non_path_zip_mem = au.mem_size();
	size_t non_path_zip_states = au.total_states();
	t1 = pf.now();
	if (write_dot) {
		std::string dotFile = std::string(finput_name?finput_name:"ftrie") + ".au.dot";
		au.patricia_trie_write_dot_file(dotFile.c_str());
	}
	au.give_to(&pz);
	if (g_minZpathLen)
		pz.path_zip("DFS", g_minZpathLen);
	pz.set_is_dag(true);
	t2 = pf.now();
	if (write_dot) {
		std::string dotFile = std::string(finput_name?finput_name:"ftrie") + ".pz.dot";
		pz.patricia_trie_write_dot_file(dotFile.c_str());
	}
	if (!be_quiet) {
		fprintf(stderr, "record_num[all=%zd uniq=%zd] record_bytes[all=%zd uniq=%zd] mem_size=%zd non_path_zip_mem=%zd\n"
			, record_num_all, record_num_uniq
			, record_bytes_all, record_bytes_uniq
			, pz.mem_size()
			, non_path_zip_mem
			);
		fprintf(stderr, "states: non_path_zip=%zd path_zip=%zd\n", non_path_zip_states, pz.total_states());
		fprintf(stderr, "time: read+insert=%f's path_zip=%f\n", pf.sf(t0,t1), pf.sf(t1,t2));
	}
	return 0;
}

template<class FastPzDFA>
void do_save_fast_dfa(const FastPzDFA& dfa) {
	if (!be_quiet)
		fprintf(stderr, "%s\n", BOOST_CURRENT_FUNCTION);
	if (save_as_mmap) {
		if (!be_quiet)
			fprintf(stderr, "write fast dfa mmap file: %s\n", xdfa_file);
		dfa.save_mmap(xdfa_file);
	}
	else {
		if (!be_quiet)
			fprintf(stderr, "write fast dfa file: %s\n", xdfa_file);
		dfa.save_to(xdfa_file);
	}
}
template<class FastPzDFA>
void save_fast_dfa_aux(const MainPzDFA& src, FastPzDFA*) {
	FastPzDFA dst; dst.copy_with_pzip(src);
	dst.set_is_dag(true);
	do_save_fast_dfa(dst);
}
void save_fast_dfa_aux(const MainPzDFA& src, MainPzDFA*) {
	do_save_fast_dfa(src);
}

void save_fast_dfa(const MainPzDFA& pz) {
	for(size_t expand = 1; expand < 512; expand *= 16) {
		size_t dmax = pz.total_states() * expand;
		try {
		#define save_fast_dfa_if_fits(State) \
			else if (dmax < State::max_state) { \
				save_fast_dfa_aux(pz, (DFA_Template<State>*)NULL); break; }
			if (32 == state_int_name) {
				save_fast_dfa_aux(pz, (MainPzDFA*)NULL);
				return;
			}
			save_fast_dfa_if_fits(State4B)
			save_fast_dfa_if_fits(State5B)
			save_fast_dfa_if_fits(State6B)
			save_fast_dfa_if_fits(State32)
		#if TERARK_WORD_BITS >= 64 && defined(TERARK_INST_ALL_DFA_TYPES)
		  #if !defined(_MSC_VER)
			save_fast_dfa_if_fits(State34)
		  #endif
			save_fast_dfa_if_fits(State7B)
			save_fast_dfa_if_fits(State38)
		#endif
			fprintf(stderr
			  , "Fatal: SaveFastDFA: MainPzDFA.states=%zd expand=%zd\n"
			  , pz.total_states(), expand);
			return;
		}
		catch (const std::out_of_range&) {
			if (!be_quiet)
				fprintf(stderr, "SaveFastDFA: try larger state\n");
		}
	}
}

template<template<int,int> class Ldfa>
void save_linear_dfa(const MainPzDFA& pz, const char* foutput) {
	for(size_t expand = 4; expand < 512; expand *= 16) {
		size_t dmax = pz.total_states() * expand;
		try {
		#define save_linear_if_fit(StateBytes) \
			if (dmax < Ldfa<StateBytes, 256>::max_state) { \
				do_save_linear_dfa<Ldfa<StateBytes, 256> >(pz, foutput); \
				return; }
		#if defined(TERARK_INST_ALL_DFA_TYPES)
			save_linear_if_fit(2)
			save_linear_if_fit(3)
		#endif
			save_linear_if_fit(4)
			save_linear_if_fit(5)
		#if TERARK_WORD_BITS >= 64 && !defined(_MSC_VER)
			save_linear_if_fit(6)
		#endif
		#if TERARK_WORD_BITS >= 64 && defined(TERARK_INST_ALL_DFA_TYPES)
			save_linear_if_fit(7)
			save_linear_if_fit(8)
		#endif
			// Unexpected:
			fprintf(stderr
			  , "Fatal: SaveLinearDFA: MainPzDFA.states=%zd expand=%zd\n"
			  , pz.total_states(), expand);
			return;
		}
		catch (const std::out_of_range&) {
			if (!be_quiet)
				fprintf(stderr, "SaveLinearDFA: try larger state\n");
		}
	}
}

int main(int argc, char* argv[]) {
	g_zp_conf.initFromEnv();
	g_zp_conf.nestLevel = 0; // disable zpath link to nest louds trie
	for (;;) {
		int opt = getopt(argc, argv, "b:co:O:Q:S:U:u:l:tm::sqz:w:g");
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
			state_int_name = atoi(optarg);
			break;
		case 'c':
			check_ldfa = true;
			break;
		case 'g':
			write_dot = true;
			break;
		case 'o':
			ldfa_file = optarg;
			break;
		case 'O':
			xdfa_file = optarg;
			break;
		case 'Q':
			qdfa_file = optarg;
			break;
		case 'S':
			sdfa_file = optarg;
			break;
		case 'U': // Ultra small, louds dfa
			udfa_file = optarg;
			break;
		case 'u': // Ultra small, LoudsDFA_IL
			udfa_file_il = optarg;
			break;
		case 't':
			write_text = true;
			break;
		case 'l':
		  {
			if (isdigit(optarg[0])) {
				fprintf(stderr, "-l linear_state_bytes is ignored\n");
			}
			const char* colon = strchr(optarg, ':');
			if (colon)
				ldfa_walk_method = colon + 1;
			break;
		  }
		case 'm':
			if (optarg && atoi(optarg) == 0) save_as_mmap = false;
			break;
		case 'q':
			be_quiet = true;
			break;
		case 's':
			is_input_sorted = 1;
			break;
		case 'w':
			if (strcasecmp(optarg, "dup") == 0)
				warn_dup = true;
			break;
		case 'z':
			g_minZpathLen = atoi(optarg);
			g_minZpathLen = std::max(g_minZpathLen, 0);
			break;
		}
	}
GetoptDone:
	if (optind < argc) {
		finput_name = argv[optind];
	}
	if (0 != strcasecmp(ldfa_walk_method, "BFS") &&
		0 != strcasecmp(ldfa_walk_method, "DFS") &&
		0 != strcasecmp(ldfa_walk_method, "PFS") &&
		1) {
		fprintf(stderr, "WalkMethod of -l StateBytes:WalkMethod must be BFS, DFS, or PFS\n");
		return 3;
	}
	if (!xdfa_file && !ldfa_file && !qdfa_file && !sdfa_file) {
		fprintf(stderr, "At least one of\n"
			"\t-O xfast_dfa_file or -o small_linear_dfa_file or\n"
			"\t-Q quick_dfa_file or -S small_quick__dfa_file is required\n");
		return 3;
	}
	mapred_job_id = getenv("mapred_job_id");
	int ret = 0;
	try {
		MainPzDFA pz;
		ret = is_input_sorted ? build<1>(pz) : build<0>(pz);
		if (xdfa_file)
			save_fast_dfa(pz);
		if (ldfa_file)
			save_linear_dfa<LinearDFA>(pz, ldfa_file);
		if (qdfa_file)
			save_linear_dfa< QuickDFA>(pz, qdfa_file);
		if (sdfa_file)
			save_linear_dfa<SQuickDFA>(pz, sdfa_file);
		if (udfa_file)
			do_save_linear_dfa<LoudsDFA_SE>(pz, udfa_file);
		if (udfa_file_il)
			do_save_linear_dfa<LoudsDFA_IL>(pz, udfa_file_il);
	}
	catch (const std::exception& ex) {
		fprintf(stderr, "failed: caught exception: %s\n", ex.what());
		ret = 1;
	}
	if (!be_quiet)
		malloc_stats();
	return ret;
}

