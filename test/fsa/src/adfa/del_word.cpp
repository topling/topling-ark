#include <terark/fsa/dawg.hpp>
#include <terark/fsa/onfly_minimize.hpp>
#include <terark/util/linebuf.hpp>
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
const char* delprefix = NULL;
size_t delfact = 100;

template<class Au>
int run() {
	printf("%s\n", BOOST_CURRENT_FUNCTION);
	terark::LineBuf line;
	terark::profiling pf;
	long lineno = 0;
	long long t0, t1, t2;
	size_t pattern_bytes_all  = 0;
	size_t pattern_bytes_uniq = 0;
	size_t pattern_num_all  = 0;
	size_t pattern_num_uniq = 0;
	size_t num_words1;
	valvec<std::string> dels;
	std::string suffix_buf;
	Au au; {
		MinADFA_OnflyBuilder<Au> onfly(&au);
		t0 = pf.now();
		while (line.getline(stdin) >= 0) {
			lineno++;
			line.trim();
			if (onfly.add_word(line))
				pattern_num_uniq++, pattern_bytes_uniq += line.n;
			if (lineno % 100000 == 0) {
				printf("lineno=%ld, DFA[mem_size=%ld states=%ld transitions=%ld]\n"
						, lineno
						, (long)au.mem_size()
						, (long)au.total_states()
						, (long)au.total_transitions()
						);
			}
			pattern_num_all++;
			pattern_bytes_all += line.n;
		}
		num_words1 = au.dag_pathes();
		au.tpl_for_each_word([&](size_t nth, fstring w, size_t) {
			if (nth % delfact == 0)
				dels.push_back(std::string(w.data(), w.size()));
		});
		for (size_t i = 0; i < dels.size(); ++i) {
			bool ret = onfly.del_word(dels[i]);
		//	printf("del_word(%s)=%d\n", dels[i].c_str(), ret);
			printf("ret=%d dels[%06zd]=%s\n", ret, i, dels[i].c_str());
			assert(ret);
		}
		if (delprefix) {
			printf("delete prefix: %s\n", delprefix);
			onfly.del_by_prefix(delprefix,
				[&](fstring16 suffix) {
					suffix_buf.resize(0);
					std::copy(suffix.begin(), suffix.end(), std::back_inserter(suffix_buf));
					printf("del_by_prefix: %s|%s\n", delprefix, suffix_buf.c_str());
					return true;
				});
		}
	}
	size_t non_path_zip_size = au.mem_size();
	t1 = pf.now();
	au.path_zip("DFS");
	t2 = pf.now();
	size_t num_words2 = au.dag_pathes();
	printf("pattern_num[all=%zd uniq=%zd] pattern_bytes[all=%zd uniq=%zd] num_words1=%zd\n"
			, pattern_num_all, pattern_num_uniq
			, pattern_bytes_all, pattern_bytes_uniq
			, num_words1
			);
	printf("DFA: states=%zd num_words2=%zd mem_size=%zd non_path_zip_size=%zd\n"
			, au.total_states()
			, num_words2
			, au.mem_size()
			, non_path_zip_size
			);
	printf("deleted: delset=%zd real=%zd\n", dels.size(), num_words1 - num_words2);
	printf("time: read+insert=%f's path_zip=%f\n", pf.sf(t0,t1), pf.sf(t1,t2));
	if (outfile)
		au.save_to(outfile);
	malloc_stats();
	return 0;
}

int main(int argc, char* argv[]) {
	int state_bytes = 8;
	for (;;) {
		int opt = getopt(argc, argv, "b:o:p:");
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
		case 'd':
			delfact = atoi(optarg);
			break;
		case 'p':
			delprefix = optarg;
			break;
		case 'o':
			outfile = optarg;
			break;
		}
	}
GetoptDone:
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

