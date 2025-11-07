#define _CRT_SECURE_NO_WARNINGS
#define _SCL_SECURE_NO_WARNINGS
#include <terark/fsa/mre_match.hpp>
#include <terark/util/linebuf.hpp>
#include <terark/util/profiling.hpp>
#include <getopt.h>

#if defined(__DARWIN_C_ANSI)
	#define malloc_stats() (void)(0)
#else
	#include <malloc.h>
#endif

#ifdef _MSC_VER
	#define malloc_stats() (void)(0)
#endif

using namespace terark;

int main(int argc, char* argv[]) {
	bool verbose = false;
	bool lower_case = false;
	MultiRegexMatchOptions mrOpt;
	for (int opt=0; (opt = getopt(argc, argv, "D:i:lv")) != -1; ) {
		switch (opt) {
		case '?': return 1;
		case 'D': mrOpt.enableDynamicDFA = atoi(optarg) != 0; break;
		case 'i': mrOpt.dfaFilePath = optarg;       break;
		case 'l': lower_case = true; break;
		case 'v': verbose  = true;         break;
		}
	}
	if (mrOpt.dfaFilePath.empty()) {
		fprintf(stderr, "usage: -i dfa_file must be provided!\n");
		return 1;
	}
	mrOpt.load_dfa();
	std::unique_ptr<MultiRegexFullMatch>
				all(MultiRegexFullMatch::create(mrOpt));
	terark::profiling pf;
	long long ts = pf.now();
	all->warm_up();
	long long t0 = pf.now();
	long lineno = 0;
	long matched = 0;
	long sumlen = 0;
	long sumhit = 0;
	long all_len = 0;
	terark::LineBuf line;
	while (line.getline(stdin) > 0) {
		lineno++;
		line.chomp();
		if (lower_case)
		  //all->match_all(fstring(line), ::tolower);          // slower
			all->match_all(fstring(line), gtab_ascii_tolower); // faster
		else
			all->match_all(fstring(line));
		if (all->size()) {
			if (verbose) {
				printf("line:%ld:", lineno);
				for(int regexId : *all) {
					printf(" %d", regexId);
				}
				printf("\n");
			}
			matched++;
			sumlen += line.size();
			sumhit += all->size();
		}
		all_len += line.size();
	}
	long long t1 = pf.now();
	printf("time(warm_up)=%f's\n", pf.sf(ts, t0));
	printf("time=%f's lines=%ld matched=%ld QPS=%f Throughput=%f'MiB Latency=%f'us\n"
			, pf.sf(t0,t1)
			, lineno
			, matched
			, lineno/pf.sf(t0,t1)
			, sumlen/pf.uf(t0,t1)
			, pf.uf(t0,t1)/lineno
			);
	printf("sum hit num = %zd, %.1f Hits Per Second\n", sumhit, sumhit / pf.sf(t0,t1));
	printf("time=%f's lines=%ld all(includes missed)=%ld QPS=%f Throughput=%f'MiB Latency=%f'us\n"
			, pf.sf(t0,t1)
			, lineno
			, all_len
			, lineno/pf.sf(t0,t1)
			, all_len/pf.uf(t0,t1)
			, pf.uf(t0,t1)/lineno
			);
	malloc_stats();
	return 0;
}

