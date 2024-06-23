#include <terark/fsa/dawg.hpp>
#include <terark/fsa/onfly_minimize.hpp>
#include <terark/util/profiling.hpp>
#include <terark/util/linebuf.hpp>
#include <getopt.h>

using namespace terark;

void usage(const char* prog) {
	fprintf(stderr, "usage: %s -o output_dawg_file -b state_bytes -c(check)\n", prog);
}

terark::profiling pf;
char* ofname = NULL;
bool  check_file = false;

template<class State>
void run() {
	DAWG<State> dawg1;
	MinADFA_OnflyBuilder<DAWG<State> > onfly(&dawg1);
	terark::LineBuf line;
	long lines = 0;
	long bytes = 0;
	auto print = [&]() {
		printf("lineno=%09ld, DAWG[mem_size=%ld states=%ld transitions=%ld]\n"
				, lines
				, (long)dawg1.mem_size()
				, (long)dawg1.total_states()
				, (long)dawg1.total_transitions()
				);
	};
	long long t0 = pf.now();
	for (; line.getline(stdin) >= 0; ++lines) {
		line.trim();
		if (line.n)
			onfly.add_word(line);
		if (lines % 100000 == 0)
		   	print();
		bytes += line.n;
	}
	long long t1 = pf.now();
	print();
	printf("path_zipping...\n");
	dawg1.path_zip("DFS");
	long long t2 = pf.now();
	printf("compiling...\n");
	dawg1.compile();
	long long t3 = pf.now();
	if (ofname) {
		printf("saving to file %s ...\n", ofname);
		dawg1.save_to(ofname);
	}
	long long t4 = pf.now();
	if (check_file) {
		using namespace terark;
		printf("loading from file %s ...\n", ofname);
		std::auto_ptr<BaseDFA> x(BaseDFA::load_from(ofname));
		const DAWG<State>& dawg2 = dynamic_cast<const DAWG<State>&>(*x);
		if (dawg1.total_states() != dawg2.total_states()) {
			fprintf(stderr, "check failed: states[this=%ld file=%ld]\n"
					, (long)dawg1.total_states()
				   	, (long)dawg2.total_states()
					);
		}
		if (dawg1.mem_size() != dawg2.mem_size()) {
			fprintf(stderr, "check failed: mem_size[this=%ld file=%ld]\n"
					, (long)dawg1.mem_size()
				   	, (long)dawg2.mem_size()
					);
		}
		if (memcmp(dawg1.internal_get_pool().data()
				 , dawg2.internal_get_pool().data()
				 , dawg1.internal_get_pool().size()
				 ) != 0) {
			fprintf(stderr, "check failed: memcmp pool didn't match\n");
		}
		if (memcmp(dawg2.internal_get_states().data()
				 , dawg1.internal_get_states().data()
				 , dawg1.total_states() * sizeof(State)
				 ) != 0) {
			fprintf(stderr, "check failed: memcmp states didn't match\n");
		}
	}
	long long t5 = pf.now();
	long words = dawg1.dag_pathes();
	long long t6 = pf.now();
	printf("time: insert=%f path_zip=%f compile=%f save=%f check=%f dag_pathes=%f\n"
			, pf.sf(t0,t1)
			, pf.sf(t1,t2)
			, pf.sf(t2,t3)
			, pf.sf(t3,t4)
			, pf.sf(t4,t5)
			, pf.sf(t5,t6)
			);
	printf("lines=%ld unique=%ld\n", lines, words);
	printf("ThroughPut = %f'MB/s (insert + path_zip + compile)\n", bytes/pf.uf(t0, t3));
	printf("time_ns_per_insert_word=%f\n", pf.nf(t0,t3)/lines);
	printf("time_ns_per_insert_byte=%f\n", pf.nf(t0,t3)/bytes);
}

int main(int argc, char** argv) {
    using namespace std;
	int state_bytes = 8;
	for (int opt=0; (opt = getopt(argc, argv, "b:co:")) != -1; ) {
		switch (opt) {
			case 'b':
				state_bytes = atoi(optarg);
				break;
			case 'o':
				ofname = optarg;
				break;
			case 'c':
				check_file = true;
				break;
			case '?':
				usage(argv[0]);
				return 3;
		}
	}
	switch (state_bytes) {
	case 4: run<State4B>(); break;
	case 5: run<State5B>(); break;
	case 6: run<State6B>(); break;
//	case 7: run<State7B>(); break;
	case 8: run<State32>(); break;
	}
    return 0;
}


