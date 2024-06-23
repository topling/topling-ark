#define _CRT_SECURE_NO_WARNINGS
#define _SCL_SECURE_NO_WARNINGS

#include <terark/fsa/dawg.hpp>
#include <terark/util/profiling.hpp>
#include <terark/io/DataIO.hpp>
#include <terark/io/FileStream.hpp>
#include <terark/io/StreamBuffer.hpp>
#include <getopt.h>
#include <fcntl.h>

#ifndef _MSC_VER
#include <sys/resource.h>
#endif

using namespace terark;

template<class State, class DataIO>
static void Unzip(DataIO& dio, uint64_t version) {
	Automata<State> au;
	au.load_au(dio, version);
	if (!au.tpl_is_dag()) {
		throw std::logic_error("Loaded DFA is not a DAG\n");
	}
	au.tpl_for_each_word([](size_t nth, fstring word, size_t /*acc_state*/) {
		*(char*)word.end() = '\n'; // risk set newline
		fwrite(word.data(), 1, word.size() + 1, stdout);
		*(char*)word.end() = '\0'; // restore
	});
}
static int UnzipFile(FILE* ifp, FILE* ofp);
static int UnzipFileByIter(FILE* ifp, FILE* ofp);

using namespace terark;
bool isBson = false;
bool verbose = false;
bool useIter = false;
int main(int argc, char* argv[]) {
	valvec<const char*> ifiles;
	const char* ofname = NULL;
	for (;;) {
		int opt = getopt(argc, argv, "o:BIv");
		switch (opt) {
		case -1:
			goto GetoptDone;
		case '?':
			return 1;
		case 'o':
			ofname = optarg;
			break;
        case 'B':
            isBson = true;
            break;
		case 'I':
			useIter = true;
			break;
		case 'v':
			verbose = true;
			break;
		}
	}
GetoptDone:
	for (int j = optind; j < argc; ++j) {
		ifiles.push_back(argv[j]);
	}
	valvec<Auto_fclose> ifp(ifiles.size(), valvec_reserve());
	Auto_fclose ofp;
	if (!ifiles.empty()) {
		for (size_t j = 0; j < ifiles.size(); ++j) {
			FILE* fp = fopen(ifiles[j], "rb");
			if (NULL == fp) {
				fprintf(stderr, "fopen(%s, rb) = %s\n", ifiles[j], strerror(errno));
				return 1;
			}
			ifp.emplace_back(fp);
		}
	}
	else {
	    #ifdef _MSC_VER
		_setmode(_fileno(stdin), _O_BINARY);
	    #endif
		ifp.emplace_back(nullptr); // stdin
	}
	if (ofname) {
		ofp = fopen(ofname, isBson?"wb":"w");
		if (NULL == ofp) {
			fprintf(stderr, "fopen(%s, w) = %s\n", ofname, strerror(errno));
			return 1;
		}
	}
#if 0 // now unzip
	const rlim_t kStackSize = 64L * 1024L * 1024L; // 64MB
	struct rlimit rl;
	int result = getrlimit(RLIMIT_STACK, &rl);
	if (result == 0) {
		if (rl.rlim_cur < kStackSize) {
			rl.rlim_cur = kStackSize;
			result = setrlimit(RLIMIT_STACK, &rl);
		}
	}
	if (result != 0) {
		fprintf(stderr, "WARNING: setrlimit(StackSize: %ldMB) = %d : %s\n"
			, long(kStackSize)/(1024L*1024L), result, strerror(errno));
		fprintf(stderr, "WARNING: The program maybe failed with stack overflow!\n");
	}
#endif // _MSC_VER

	for (size_t j = 0; j < ifp.size(); ++j) {
		auto UnzipFile = useIter ? UnzipFileByIter : ::UnzipFile;
		int err = UnzipFile(ifp[j].self_or(stdin), ofp.self_or(stdout));
		if (err)
			return err;
	}
	return 0;
}

static int UnzipFile(FILE* ifp, FILE* ofp) {
	int ret = 0;
	NonOwnerFileStream file(ifp);
	NativeDataInput<InputBuffer> dio; dio.attach(&file);
	var_uint64_t version;
	std::string state_name;
#if defined(COMPATIBLE_WITH_UNWRAPPED_DFA)
	dio >> version;
	if (version.t > 5) {
		goto DFA_is_wrapped;
	}
	dio >> state_name;
#define ON_STATE(State) \
	if (typeid(State).name() == state_name) { \
		if (verbose) \
			fprintf(stderr, "state_name: %-30s : %s\n", state_name.c_str(), #State); \
		Unzip<State>(dio, version.t); \
		return 0; \
	}
//-------------------------
	ON_STATE(State4B)
	ON_STATE(State5B)
	ON_STATE(State6B)
	ON_STATE(State32)
#if TERARK_WORD_BITS >= 64 && !defined(_MSC_VER)
	ON_STATE(State34)
#endif
	ON_STATE(DAWG_State<State4B>)
	ON_STATE(DAWG_State<State5B>)
	ON_STATE(DAWG_State<State6B>)
	ON_STATE(DAWG_State<State32>)
#if TERARK_WORD_BITS >= 64 && defined(TERARK_INST_ALL_DFA_TYPES)
	ON_STATE(State7B)
	ON_STATE(DAWG_State<State7B>)
#endif
	DFA_is_wrapped:
#endif
	try {
		std::string className;
		dio.risk_set_bufpos(0);
		dio >> className;
		dio.risk_set_bufpos(0);
		if (verbose)
			fprintf(stderr, "use BaseDFA::load_from, DFA_class=%s\n", className.c_str());
		std::unique_ptr<BaseDFA> dfa(BaseDFA::load_from_NativeInputBuffer(&dio));
		if (dfa) {
			if (!dfa->is_dag()) {
				throw std::logic_error("Loaded DFA is not a DAG\n");
			}
			AcyclicPathDFA* adfa = dynamic_cast<AcyclicPathDFA*>(dfa.get());
			if (adfa) {
                if (isBson) {
                	NonOwnerFileStream ofile(ofp);
                    LittleEndianDataOutput<OutputBuffer> obuf;
                    ofile.disbuf();
                    obuf.attach(&ofile);
                    adfa->for_each_word([&obuf](size_t/*nth*/, fstring str) {
                        obuf << uint32_t(4 + str.n);
                        obuf.ensureWrite(str.p, str.n);
                    });
                }
                else {
    				adfa->print_output(ofp);
                }
			} else {
				fprintf(stderr, "className=%s is not an AcyclicPathDFA\n", state_name.c_str());
			}
		} else {
			fprintf(stderr, "BaseDFA::load_from(stdin) failed: className=%s\n", state_name.c_str());
			ret = 3;
		}
	}
	catch (const std::exception& ex) {
		fprintf(stderr, "exception: %s\n", ex.what());
	}
	return ret;
}

static int UnzipFileByIter(FILE* ifp, FILE* ofp) {
	int ret = 0;
	NonOwnerFileStream file(ifp);
	NativeDataInput<InputBuffer> dio; dio.attach(&file);
	var_uint64_t version;
	try {
		std::string className;
		dio.risk_set_bufpos(0);
		dio >> className;
		dio.risk_set_bufpos(0);
		if (verbose)
			fprintf(stderr, "use BaseDFA::load_from, DFA_class=%s\n", className.c_str());
		std::unique_ptr<BaseDFA> dfa(BaseDFA::load_from_NativeInputBuffer(&dio));
		if (!dfa) {
			fprintf(stderr, "BaseDFA::load_from(stdin) failed: className=%s\n", className.c_str());
			return 3;
		}
		if (!dfa->is_dag()) {
			fprintf(stderr, "Loaded DFA is not a DAG: className=%s\n", className.c_str());
		}
		AcyclicPathDFA* adfa = dynamic_cast<AcyclicPathDFA*>(dfa.get());
		if (!adfa) {
			fprintf(stderr, "className=%s is not an AcyclicPathDFA\n", className.c_str());
			return 1;
		}
		auto iter = adfa->adfa_make_iter();
		bool isBsonForLoop = isBson; // make compiler optimize well
		NonOwnerFileStream ofile(ofp);
		LittleEndianDataOutput<OutputBuffer> obuf;
		if (isBsonForLoop) {
			ofile.disbuf();
			obuf.attach(&ofile);
		}
		if (iter->seek_begin()) {
			do {
				if (isBsonForLoop) {
					fstring str = iter->word();
					obuf << uint32_t(4 + str.n);
					obuf.ensureWrite(str.p, str.n);
				} else {
					auto& w = iter->mutable_word();
					w.ensure_unused(1)[0] = '\n';
					obuf.ensureWrite(w.data(), w.size() + 1);
				}
			} while (iter->incr());
		}
		iter->dispose();
	}
	catch (const std::exception& ex) {
		fprintf(stderr, "exception: %s\n", ex.what());
	}
	return ret;
}

