#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#define _SCL_SECURE_NO_WARNINGS
#endif
#include <terark/fsa/dawg.hpp>
#include <terark/fsa/linear_dfa.hpp>
#include <terark/fsa/onfly_minimize.hpp>
#include <terark/util/profiling.hpp>
#include <terark/util/autoclose.hpp>
#include <getopt.h>

#if defined(__DARWIN_C_ANSI)
	#define malloc_stats() (void)(0)
#else
	#include <malloc.h>
#endif

#ifdef _MSC_VER
	#include <fcntl.h>
	#include <io.h>
	#define strcasecmp _stricmp
	#define malloc_stats() (void)(0)
#endif

using namespace terark;

typedef Automata<State32_512> MainDFA;
typedef MainDFA MainPzDFA;

void usage(const char* prog) {
	fprintf(stderr, "usage: %s [-b state_size]\n", prog);
	exit(3);
}

terark::profiling pf;

const char* finput_name = NULL;
const char* outfile = NULL;
const char* ldfa_outfile = NULL;
const char* ldfa_walk_method = "BFS";
bool save_as_mmap = true;
bool warn_dup = false;
bool be_quiet = false;
int state_int_name = -1;
int is_input_sorted = 0;
int g_minZpathLen = 2;

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
	if (save_as_mmap) {
		if (!be_quiet)
			fprintf(stderr, "write (small) linear_dfa mmap file: %s\n", ldfa_outfile);
		ldfa.save_mmap(ldfa_outfile);
	} else {
		if (!be_quiet)
			fprintf(stderr, "write (small) linear_dfa file: %s\n", ldfa_outfile);
		ldfa.save_to(ldfa_outfile);
	}
}

template<bool IsInputSorted>
int build(MainPzDFA& pz) {
	if (!be_quiet)
		fprintf(stderr, "%s\n", BOOST_CURRENT_FUNCTION);
	long long t0, t1, t2;
	uint32_t kvlen[2];
	size_t record_bytes_all  = 0;
	size_t record_bytes_uniq = 0;
	size_t record_num_all  = 0;
	size_t record_num_uniq = 0;
	valvec<char> kvbuf;
	MainDFA au; {
		FILE* finput = stdin;
		if (finput_name) {
			finput = fopen(finput_name, "rb");
			if (NULL == finput) {
				fprintf(stderr, "failed: fopen(%s, r) = %s\n", finput_name, strerror(errno));
				return 3;
			}
		} else {
		    #ifdef _MSC_VER
			if (_setmode(_fileno(stdin), _O_BINARY) < 0) {
				THROW_STD(invalid_argument, "set stdin as binary mode failed");
			}
		    #endif
			if (!be_quiet)
				fprintf(stderr, "reading from stdin\n");
		}
		typename boost::mpl::if_c<IsInputSorted
			, MinADFA_OnflyBuilder<MainDFA>::Ordered
			, MinADFA_OnflyBuilder<MainDFA>
			>::type
		onfly(&au);
		t0 = pf.now();
		while (fread(&kvlen, 1, sizeof(kvlen), finput) == sizeof(kvlen)) {
			kvbuf.resize_no_init(kvlen[0] + kvlen[1]);
			size_t nRead = fread(kvbuf.data(), 1, kvbuf.size(), finput);
			if (kvbuf.size() != nRead) {
				fprintf(stderr, "unexpected: nRequest=%zd nReaded=%zd\n", kvbuf.size(), nRead);
				break;
			}
			fstring key(kvbuf.data(), kvlen[0]);
			fstring val(kvbuf.data() + kvlen[0], kvlen[1]);
			if (onfly.add_key_val(key, val))
				record_num_uniq++, record_bytes_uniq += kvbuf.size();
			if (!be_quiet && record_num_all % 100000 == 0) {
				fprintf(stderr, "record_num=%zd, DFA[mem_size=%zd states=%zd transitions=%zd]\n"
						, record_num_all
						, au.mem_size()
						, au.total_states()
						, au.total_transitions()
						);
			}
			record_num_all++;
			record_bytes_all += kvbuf.size();
		}
	}
	if (!be_quiet)
		fprintf(stderr, "record_num=%zd, DFA[mem_size=%zd states=%zd transitions=%zd]\n"
			, record_num_all
			, au.mem_size()
			, au.total_states()
			, au.total_transitions()
			);
	size_t non_path_zip_mem = au.mem_size();
	size_t non_path_zip_states = au.total_states();
	t1 = pf.now();
	if (g_minZpathLen)
		au.path_zip("DFS", g_minZpathLen);
	au.set_is_dag(true);
	t2 = pf.now();
	if (!be_quiet) {
		fprintf(stderr, "record_num[all=%zd uniq=%zd] record_bytes[all=%zd uniq=%zd] mem_size=%zd non_path_zip_mem=%zd\n"
			, record_num_all, record_num_all
			, record_bytes_all, record_bytes_uniq
			, au.mem_size()
			, non_path_zip_mem
			);
		fprintf(stderr, "states: non_path_zip=%zd path_zip=%zd\n", non_path_zip_states, au.total_states());
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
				save_fast_dfa_aux(pz, (Automata<State>*)NULL); break; }
			if (32 == state_int_name) {
				save_fast_dfa_aux(pz, (MainPzDFA*)NULL);
				return;
			}
			save_fast_dfa_if_fits(State4B_448)
		#if !defined(_MSC_VER)
			save_fast_dfa_if_fits(State5B_448)
		#endif
			save_fast_dfa_if_fits(State6B_512)
			save_fast_dfa_if_fits(State32_512)
		#if TERARK_WORD_BITS >= 64 && defined(TERARK_INST_ALL_DFA_TYPES)
			save_fast_dfa_if_fits(State7B_512)
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
			if (dmax < LinearDFA<StateBytes, 512>::max_state) { \
				compress_to_linear<LinearDFA<StateBytes, 512> >(pz); \
				return; }
		#if defined(TERARK_INST_ALL_DFA_TYPES)
			save_linear_if_fit(2)
			save_linear_if_fit(3)
		#endif
			save_linear_if_fit(4)
		#if !defined(_MSC_VER)
			save_linear_if_fit(5)
		#endif
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
		int opt = getopt(argc, argv, "b:o:O:l:m::sqz:w:");
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
		case 'o':
			ldfa_outfile = optarg;
			break;
		case 'O':
			outfile = optarg;
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
	if (NULL == outfile && NULL == ldfa_outfile) {
		fprintf(stderr, "-O fast_dfa_file or -o small_dfa_file is required\n");
		return 3;
	}
	mapred_job_id = getenv("mapred_job_id");
	int ret = 0;
	try {
		MainPzDFA pz;
		ret = is_input_sorted ? build<1>(pz) : build<0>(pz);
		if (outfile)
			save_fast_dfa(pz);
		if (ldfa_outfile)
			save_linear_dfa(pz);
	}
	catch (const std::exception& ex) {
		fprintf(stderr, "failed: caught exception: %s\n", ex.what());
		ret = 1;
	}
	if (!be_quiet)
		malloc_stats();
	return ret;
}

