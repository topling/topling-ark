#define _CRT_SECURE_NO_WARNINGS
#define _SCL_SECURE_NO_WARNINGS

#include <terark/fsa/automata.hpp>
#include <terark/fsa/linear_dfa.hpp>
#include <terark/fsa/onfly_minimize.hpp>
#include <terark/num_to_str.hpp>
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
const char* outfile = NULL;
const char* ldfa_outfile = NULL;
const char* ldfa_walk_method = "BFS";
const char* true_text_file = NULL;
bool write_text = false;
bool check_ldfa = false;
bool save_as_mmap = true;
bool do_path_zip = true;
bool warn_dup = false;
bool be_quiet = false;
int state_int_name = -1;

const char* mapred_job_id; // check for if running in hadoop map reduce

template<class Ldfa, class Au>
void compress_to_linear(const Au& au) {
	if (!be_quiet)
		fprintf(stderr, "%s\n", BOOST_CURRENT_FUNCTION);
	long long t1 = pf.now();
	Ldfa ldfa;
	ldfa.build_from(ldfa_walk_method, au);
	ldfa.set_is_dag(true);
	long long t2 = pf.now();
	if (!be_quiet)
		fprintf(stderr, "linear_dfa: states=%zd mem_size=%zd time=%f's\n", ldfa.total_states(), ldfa.mem_size(), pf.sf(t1,t2));
#if defined(WITH_CHECK_LINEAR)
	if (!be_quiet && check_ldfa) {
		fprintf(stderr, "checking ldfa...\n");
		long long t3;
		long cnt = 0;
		const int loop = 10;
		t1 = pf.now();
		au.tpl_for_each_word([&](size_t nth, fstring w, size_t) {
				bool exists;
				for (int i = 0; i < loop; ++i)
					exists = ldfa.accept(w);
				if (!exists) {
					fprintf(stderr, "nth=%zd accept=false: %s\n", nth, w.p);
				}
				cnt++;
			});
		t2 = pf.now();
		au.tpl_for_each_word([&](size_t nth, fstring w, size_t) {
				bool exists;
				for (int i = 0; i < loop; ++i)
					exists = ldfa.accept(w);
				if (!exists) {
					fprintf(stderr, "nth=%zd accept=false: %s\n", nth, w.p);
				}
			});
		t3 = pf.now();
		fprintf(stderr, "time: ldfa=%f terark-dfa=%f, speedup=%f\n", pf.sf(t1,t2), pf.sf(t2, t3), pf.sf(t1,t2)/pf.sf(t2, t3));
		fprintf(stderr, "mQPS: ldfa=%f terark-dfa=%f\n", cnt*loop/pf.uf(t1,t2), cnt*loop/pf.uf(t2, t3));
		fprintf(stderr, "ns/Q: ldfa=%f terark-dfa=%f\n", pf.nf(t1,t2)/loop/cnt, pf.nf(t2, t3)/cnt/loop);
	}
#endif
	if (save_as_mmap) {
		if (!be_quiet)
			fprintf(stderr, "write (small) linear_dfa mmap file: %s\n", ldfa_outfile);
		ldfa.save_mmap(ldfa_outfile);
	} else {
		if (!be_quiet)
			fprintf(stderr, "write (small) linear_dfa file: %s\n", ldfa_outfile);
		ldfa.save_to(ldfa_outfile);
	}
	if (write_text) {
		au.print_output("output.txt.au");
		ldfa.print_output("output.txt.ldfa");
	}
}


int build(MainPzDFA& pz) {
	if (!be_quiet)
		fprintf(stderr, "%s\n", BOOST_CURRENT_FUNCTION);
	long long t0, t1, t2;
	size_t lineno = 0;
	size_t db_bytes_all = 0;
	Auto_fclose ttf;
	if (true_text_file) {
		ttf = fopen(true_text_file, "w");
		if (!ttf) {
			fprintf(stderr, "failed: fopen(%s, w) = %s\n", true_text_file, strerror(errno));
			return 3;
		}
	}
	MainDFA au; {
		Auto_fclose finput;
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
		MinADFA_OnflyBuilder<MainDFA> onfly(&au);
		t0 = pf.now();
		LineBuf line;
		MainDFA cols_au;
		MinADFA_OnflyBuilder<MainDFA> cols_build(&cols_au);
		valvec<fstring> F;
		byte_t delim = '\t';
		string_appender<> sbuf;
		while (line.getline(finput.self_or(stdin)) >= 0) {
			lineno++;
			line.chomp();
			if (0 == line.n) continue;
			line.split(delim, &F);
			fstring key = F[0];
			cols_au.erase_all();
			cols_build.reset_root_state(initial_state);
			for (size_t i = 1; i < F.size(); ++i) {
				if (ttf) {
					fprintf(ttf, "%.*s%c%.*s%c%zd\n"
							, int(key.n), key.p
							, delim
							, int(F[i].n), F[i].p
							, delim
							, i);
				}
				sbuf.resize(0);
				sbuf.append(F[i].p, F[i].n);
				sbuf.push_back(delim);
				sbuf << i;
				cols_build.add_word(sbuf);
			}
			auto root2 = au.copy_from(cols_au);
			onfly.add_suffix_adfa(key, delim, root2);
			if (lineno % 10000 == 0) {
				fprintf(stderr, "lineno=%zd, DFA[mem_size=%zd states=%zd transitions=%zd]\n"
						, lineno
						, au.mem_size()
						, au.total_states()
						, au.total_transitions()
						);
			}
			db_bytes_all += line.n;
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
	au.give_to(&pz);
	if (do_path_zip)
		pz.path_zip("DFS");
	pz.set_is_dag(true);
	t2 = pf.now();
	if (!be_quiet) {
		fprintf(stderr, "text[rows=%zd bytes=%zd] dfa[mem_size=%zd non_path_zip_mem=%zd]\n"
			, lineno, db_bytes_all
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
			fprintf(stderr, "write fast dfa mmap file: %s\n", outfile);
		dfa.save_mmap(outfile);
	}
	else {
		if (!be_quiet)
			fprintf(stderr, "write fast dfa file: %s\n", outfile);
		dfa.save_to(outfile);
	}
}
template<class FastPzDFA>
void save_fast_dfa_aux(const MainPzDFA& src, FastPzDFA*) {
	FastPzDFA dst; dst.copy_with_pzip(src);
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

void save_linear_dfa(const MainPzDFA& pz) {
	for(size_t expand = 4; expand < 512; expand *= 16) {
		size_t dmax = pz.total_states() * expand;
		try {
		#define save_linear_if_fit(StateBytes) \
			if (dmax < LinearDFA<StateBytes, 256>::max_state) { \
				compress_to_linear<LinearDFA<StateBytes, 256> >(pz); \
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
	for (;;) {
		int opt = getopt(argc, argv, "b:co:O:l:tm::sqZw:T:");
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
		case 'o':
			ldfa_outfile = optarg;
			break;
		case 'O':
			outfile = optarg;
			break;
		case 't':
			write_text = true;
			break;
		case 'T':
			true_text_file = optarg;
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
			fprintf(stderr, "\t-s is not supported by %s, ignored\n", argv[0]);
			break;
		case 'w':
			if (strcasecmp(optarg, "dup") == 0)
				warn_dup = true;
			break;
		case 'Z':
			do_path_zip = false;
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
	if (NULL == outfile && NULL == ldfa_outfile) {
		fprintf(stderr, "-O fast_dfa_file or -o small_dfa_file is required\n");
		return 3;
	}
	mapred_job_id = getenv("mapred_job_id");
	int ret = 0;
	try {
		MainPzDFA pz;
		ret = build(pz);
		if (0 == ret) {
			if (outfile)
				save_fast_dfa(pz);
			if (ldfa_outfile)
				save_linear_dfa(pz);
		}
	}
	catch (const std::exception& ex) {
		fprintf(stderr, "failed: caught exception: %s\n", ex.what());
		ret = 1;
	}
	if (!be_quiet)
		malloc_stats();
	return ret;
}

