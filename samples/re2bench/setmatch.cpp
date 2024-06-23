#include <re2/set.h>
#include <getopt.h>
#include <stdio.h>
#include <malloc.h>

#include <terark/fstring.hpp>
#include <terark/valvec.hpp>
#include <terark/util/autoclose.hpp>
#include <terark/util/linebuf.hpp>
#include <terark/util/profiling.hpp>

void usage(const char* prog) {
	fprintf(stderr, "%s -p pattern_file [-f text_file]\n", prog);
}

int main(int argc, char* argv[]) {
	const char* pattern_file = NULL;
	const char* text_file = NULL;
	bool verbose = false;
	int opt;
	while ((opt = getopt(argc, argv, "p:f:v")) != -1) {
		switch (opt) {
			default:
				break;
			case '?':
				usage(argv[0]);
				return 1;
			case 'p':
				pattern_file = optarg;
				break;
			case 'f':
				text_file = optarg;
				break;
			case 'v':
				verbose = true;
				break;
		}
	}
	if (NULL == pattern_file) {
		usage(argv[0]);
		return 1;
	}
	terark::Auto_fclose fp(fopen(pattern_file, "r"));
	if (!fp) {
		fprintf(stderr, "fopen(%s, r) = %s\n", pattern_file, strerror(errno));
		return 2;
	}
	terark::Auto_fclose ff;
	if (text_file) {
		ff = fopen(text_file, "r");
		if (NULL == ff) {
			fprintf(stderr, "fopen(%s, r) = %s\n", text_file, strerror(errno));
			return 2;
		}
	}
	terark::profiling pf;
	terark::LineBuf line;
	valvec<fstring> F;
	using namespace re2;
	RE2::Options ropt;
	ropt.set_case_sensitive(false);
	ropt.set_never_capture(true);
	ropt.set_max_mem(2u*1024*1024*1024-1);
	RE2::Set set(ropt, RE2::ANCHOR_START);
	std::string err;
	long t0 = pf.now();
	long lineno = 0;
	while (line.getline(fp) > 0) {
		lineno++;
		line.chomp();
		line.split('\t', &F);
		if (F.size() < 1) {
			fprintf(stderr, "ERROR: line:%ld empty line\n", lineno);
			continue;
		}
		int id = set.Add(StringPiece(F[0].p, F[0].n), &err);
		if (id < 0) {
			fprintf(stderr, "ERROR: line:%ld %s\n", lineno, err.c_str());
			continue;
		}
	}
	fprintf(stderr, "Add all regex successed: num=%ld\n", lineno);
	long t1 = pf.now();
	set.Compile();
	long t2 = pf.now();
	long matched = 0;
	std::vector<int> res;
	std::vector<StringPiece> caps;
	lineno = 0;
	while (line.getline(ff.self_or(stdin)) > 0) {
		lineno++;
		line.chomp();
		res.resize(0);
		StringPiece text(line.p, line.n);
		bool success = set.Match(text, &res);
#if 0
		for(size_t i = 0; i < res.size(); ++i) {
			int id = res[i];
			int ncap = 1 + set.GetRE2(id)->NumberOfCapturingGroups();
			caps.resize(ncap);
			set.Match(text, 0, text.size(), RE2::ANCHOR_START, caps, ncap);
			if (verbose) {
				for (int j = 0; j < ncap; ++j) {
					StringPiece s(caps[j]);
					printf("sub(%d): %.*s\n", j, s.size(), s.data());
				}
				fflush(stdout);
			}
		}
#endif
		if (success && verbose) {
			printf("line:%ld:", lineno);
			for(size_t i = 0; i < res.size(); ++i) {
				printf(" %d", res[i]);
			}
			printf("\n");
		}
		if (success)
			matched++;
	}
	long t3 = pf.now();
	printf("time(seconds): Add____=%f\n", pf.sf(t0,t1));
	printf("time(seconds): Compile=%f\n", pf.sf(t1,t2));
	printf("time(seconds): Match__=%f  lines=%ld\n", pf.sf(t2,t3), lineno);
	printf("time=%f's lines=%ld matched=%ld QPS=%f Latency=%f'us\n"
			, pf.sf(t2,t3)
			, lineno
			, matched
			, lineno/pf.sf(t2,t3)
			, pf.uf(t2,t3)/lineno
			);
	malloc_stats();
	return 0;
}

