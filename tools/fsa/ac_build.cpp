#define _CRT_SECURE_NO_WARNINGS
#define _SCL_SECURE_NO_WARNINGS

#include <terark/fsa/aho_corasick.hpp>
#include <terark/fsa/virtual_machine_dfa.hpp>
#include <terark/fsa/virtual_machine_dfa_builder.hpp>
#include <terark/util/linebuf.hpp>
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
	fprintf(stderr, R"EOS(Usage:
   %s Options [Input-Pattern-File]

Description:
   Build AC Automaton from Input-Pattern-File, if Input-Pattern-File is
   omitted, use stdin

Options:
   -O AC-Automata-File : Using general dfa trie as base trie
   -d AC-Automata-File : Using DoubleArray trie as base trie
      BaseAC::restore_word(state, word_id) can be used for
      DoubleArray trie based AC-Automata when "-e 1" was used.
   -F Full-AC-DFA-File
      Patch all fail-and-retry-success link physically, this will produce
      a physical DFA for regex ".*(P1|P2|...|Pn)", a simple stupid state
      transition table for this DFA will be very large. To reduce memory usage,
      a state compression schema is used. Since it is not needed to track the
      fail link to reach a target state, it should be much faster, but in real
      world, this is much slower, maybe caused by poor CPU cache hit rate and
      an additional indirection incured by state compression.
   -e 0, 1 or 2: default is 0
      0: Do not save any pattern data, just save AC automata self
      1: Save word length into AC automata, methods will be enabled:
         * BaseAC::wlen(word_id)
         * BaseAC::restore_word(state, word_id)
      2: Save word length and content into AC automata, methods will be enabled:
         * BaseAC::word(word_id) will be valid
   -L
      set word_id as Lexical ordinal, if not specified, word_id is set by
      input order
   -R
      Reverse pattern, for matching by MultiRegexFullMatch

Notes:
   word_id will be set as bytes lexicalgraphical ordinal number.
   word_id maybe named as pattern_id somewhere else.
)EOS"
		, prog);
}

terark::profiling pf;

const char* finput_name = NULL;
const char* outfile = NULL;
const char* daac_outfile = NULL;
const char* full_dfa_file = NULL;
bool lexical_ordering = false;
bool reverse_pattern = false;
bool write_text = false;
bool save_as_mmap = true;

// 0: don't save word length and content
// 1: save word length
// 2: save word length and content
int  ac_word_ext = 0;

template<class Au>
int run() {
	printf("%s\n", BOOST_CURRENT_FUNCTION);
	long lineno = 0;
	long long t0, t1, t2;
	size_t pattern_bytes_all  = 0;
	size_t pattern_num_all  = 0;
	Au au_ac;
  {
	terark::Auto_fclose finput;
	if (finput_name) {
		finput = fopen(finput_name, "r");
		if (NULL == finput) {
			fprintf(stderr, "failed: fopen(%s, r) = %s\n", finput_name, strerror(errno));
			return 3;
		}
	} else {
		printf("reading from stdin\n");
	}
	typename Au::ac_builder builder(&au_ac);
	t0 = pf.now();
	terark::LineBuf line;
    size_t sumlen = 0;
	while (line.getline(finput.self_or(stdin)) >= 0) {
		lineno++;
		line.chomp();
		if (0 == line.n) continue;
        if (reverse_pattern) {
            std::reverse(line.begin(), line.end());
        }
        auto ib = builder.add_word(line);
        if (ib.second && !lexical_ordering) {
            switch (ac_word_ext) {
            case 2: au_ac.mutable_strpool().append(line); no_break_fallthrough;
            case 1: no_break_fallthrough;
            case 0: au_ac.mutable_offsets().push_back(sumlen);
            }
            sumlen += line.size();
        }
		if (lineno % 100000 == 0) {
			printf("lineno=%ld, Trie[mem_size=%ld states=%ld transitions=%ld]\n"
					, lineno
					, (long)au_ac.mem_size()
					, (long)au_ac.total_states()
					, (long)au_ac.total_transitions()
					);
		}
		pattern_num_all++;
		pattern_bytes_all += line.n;
	}
    if (!lexical_ordering) {
        au_ac.mutable_offsets().push_back(sumlen);
    }
	printf("lineno=%ld, Trie[mem_size=%ld states=%ld transitions=%ld]\n"
			, lineno
			, (long)au_ac.mem_size()
			, (long)au_ac.total_states()
			, (long)au_ac.total_transitions()
			);
	t1 = pf.now();
	au_ac.set_is_dag(true);
    if (lexical_ordering) {
        switch (ac_word_ext) {
        default:
            assert(false); // will not happen
            break;
        case 0:
        {
            valvec<unsigned> offsets;
            builder.set_word_id_as_lex_ordinal(&offsets, NULL);
            builder.compile();
            builder.sort_by_word_len(offsets);
            break;
        }
        case 1:
            builder.lex_ordinal_fill_word_offsets();
            builder.compile();
            builder.sort_by_word_len();
            break;
        case 2:
            builder.lex_ordinal_fill_word_contents();
            builder.compile();
            builder.sort_by_word_len();
            break;
        }
    }
    else {
        builder.compile();
        builder.sort_by_word_len(au_ac.get_offsets());
#if !defined(NDEBUG)
        au_ac.tpl_for_each_word([&](size_t, fstring word, size_t state) {
            size_t nWords = 0;
            const uint32_t* pWords = au_ac.words(state, &nWords);
            assert(au_ac.word(pWords[0]) == word);
            for (size_t i = 1; i < nWords; ++i) {
                assert(au_ac.wlen(pWords[i]) < au_ac.wlen(pWords[i-1]));
            }
        });
#endif
        if (0 == ac_word_ext)
            au_ac.mutable_offsets().clear();
    }
	t2 = pf.now();
  }
	printf("pattern_num=%zd pattern_bytes=%zd mem_size=%zd\n"
			, pattern_num_all
			, pattern_bytes_all
			, au_ac.mem_size()
			);
	printf("time: read+insert=%f's compile=%f\n", pf.sf(t0,t1), pf.sf(t1,t2));
	if (outfile) {
		if (save_as_mmap) {
			printf("write normal AC DFA mmap file: %s\n", outfile);
			au_ac.save_mmap(outfile);
		}
		else {
			printf("write normal AC DFA file: %s\n", outfile);
			au_ac.save_to(outfile);
		}
	}
	if (daac_outfile) {
		printf("creating double array AC Automata...\n");
		t0 = pf.now();
		Aho_Corasick_DoubleArray_8B da_ac;
		double_array_ac_build_from_au(da_ac, au_ac, "DFS");
		t1 = pf.now();
		fprintf(stderr, "double_array_trie: states=%ld, transition=%ld, mem_size=%ld, time=%f's\n",
				(long)da_ac.total_states(),
				(long)da_ac.total_transitions(),
				(long)da_ac.mem_size(),
				pf.sf(t0, t1));
		da_ac.set_is_dag(true);
		if (save_as_mmap) {
			printf("write double array AC DFA mmap file: %s\n", daac_outfile);
			da_ac.save_mmap(daac_outfile);
		}
		else {
			printf("write double array AC DFA file: %s\n", daac_outfile);
			da_ac.save_to(daac_outfile);
		}
	}
	if (full_dfa_file) {
		t0 = pf.now();
		const char* colon = strchr(full_dfa_file, ':');
		if (colon) {
			printf("creating VirtualMachineDFA from FullDFA_BaseOnAC...\n");
			FullDFA_BaseOnAC<Au> fulldfa(&au_ac);
			VirtualMachineDFA vmdfa;
			vmdfa.build_from(fulldfa);
			vmdfa.save_mmap(colon + 1);
		}
		else {
			printf("creating FullDFA<uint32_t> from FullDFA_BaseOnAC...\n");
			FullDFA_BaseOnAC<Au> fulldfa(&au_ac);
			AC_FullDFA<uint32_t> realdfa;
			uint32_t src_root = initial_state;
			realdfa.build_from(fulldfa, &src_root, 1);
			realdfa.save_mmap(full_dfa_file);
		}
	}
	if (getEnvBool("PRINT_MALLOC_STATS")) {
		malloc_stats();
	}
	return 0;
}

int main(int argc, char* argv[]) {
	int state_bytes = 16;
	for (;;) {
		int opt = getopt(argc, argv, "b:d:e:O:F:htm::LR");
		switch (opt) {
		default:
			usage(argv[0]);
			return 3;
		case -1:
			goto GetoptDone;
		case '?':
			fprintf(stderr, "invalid option: %c\n", optopt);
			usage(argv[0]);
			return 3;
		case 'b':
			state_bytes = atoi(optarg);
			break;
		case 'd':
			daac_outfile = optarg;
			break;
		case 'e':
			ac_word_ext = atoi(optarg);
			if (ac_word_ext < 0 || ac_word_ext > 2) {
				fprintf(stderr, "-e ac_word_ext must be 0 or 1 or 2, default is 0\n");
				return 1;
			}
			break;
		case 'O':
			outfile = optarg;
			break;
		case 'F':
			full_dfa_file = optarg;
			break;
		case 't':
			write_text = true;
			break;
		case 'm':
			if (optarg && atoi(optarg) == 0)
				save_as_mmap = false;
			break;
        case 'L':
            lexical_ordering = true;
            break;
        case 'R':
            reverse_pattern = true;
            break;
		}
	}
GetoptDone:
	if (optind < argc) {
		finput_name = argv[optind];
	}
	if (NULL == outfile && NULL == daac_outfile) {
		fprintf(stderr, "-O normal_trie_file or -d double_array_trie_file is required\n");
		usage(argv[0]);
		return 3;
	}
	int ret = 0;
  try {
	switch (state_bytes) {
	default:
		fprintf(stderr, "invalid state_bytes=%d\n", state_bytes);
		return 3;
	case  8: ret = run<Aho_Corasick_SmartDFA_AC_State4B>(); break; // state_size==08
#if !defined(_MSC_VER)
	case 12: ret = run<Aho_Corasick_SmartDFA_AC_State5B>(); break; // state_size==12
#endif
	case 16: ret = run<Aho_Corasick_SmartDFA_AC_State32>(); break; // state_size==16
	}
	return ret;
  }
  catch (const std::exception& ex) {
	fprintf(stderr, "failed: caught exception: %s\n", ex.what());
	return 1;
  }
}

