#include <re2/filtered_re2.h>
#include <terark/fsa/aho_corasick.hpp>
#include <terark/fsa/double_array_trie.hpp>
#include <terark/util/linebuf.hpp>
#include <terark/util/profiling.hpp>
#include <terark/util/autoclose.hpp>
//#include <boost/smart_ptr/shared_ptr.hpp>
#include <getopt.h>
#include <malloc.h>

#include <terark/fsa/re2/multi_regex.hpp>

using namespace terark;

int main(int argc, char* argv[]) {
	const char* pattern_file = NULL;
	valvec<const char*> input_files;
	for (;;) {
		int opt = getopt(argc, argv, "p:f:");
		switch (opt) {
		case -1:
			goto GetoptDone;
		case 'p':
			pattern_file = optarg;
			break;
		case 'f':
			input_files.push_back(optarg);
			break;
		}
	}
GetoptDone:
	for (int i = optind; i < argc; ++i) {
	//	fprintf(stderr, "input_file: %s\n", );
		input_files.push_back(argv[i]);
	}
	using namespace terark;
	using re2::StringPiece;
	using re2::RE2;
	MultiRegex regexps;
	LineBuf line;
	valvec<std::string> actions;
	{
		Auto_fclose fp(fopen(pattern_file, "r"));
		valvec<fstring> F;
		while (line.getline(fp) > 0) {
			line.trim();
			line.split('\t', &F);
			if (F.size() < 2) continue;
			if ('#' == line[0]) continue;
			int id = -1;
			if (0 == regexps.add(StringPiece(F[0].p, F[0].n), RE2::Options(), &id)) {
				actions.push_back(F[1].str());
			}
			if (id % 10000 == 0) {
				printf("id=%d\n", id);
			}
		}
	}
	malloc_stats();
	printf("Add regex completed, compile...\n");
	regexps.compile();
	malloc_stats();
	printf("Compile completed, matching...\n");
	std::vector<int> matched_regexs;
	for (size_t i = 0; i < input_files.size(); ++i) {
		const char* fname = input_files[i];
		Auto_fclose fp(fopen(fname, "r"));
		if (NULL == fp) {
			fprintf(stderr, "failed: fopen(\"%s\", \"r\") = %s\n", fname, strerror(errno));
			continue;
		}
		while (line.getline(fp) > 0) {
			line.trim();
			regexps.scan(re2::StringPiece(line.p, line.n), &matched_regexs);
			printf("%s\n", line.p);
			for (size_t j = 0; j < matched_regexs.size(); ++j) {
				int regex_id = matched_regexs[j];
				printf("\tMatched: %s\t%s\n", regexps.GetRE2(regex_id)->pattern().c_str(), actions[regex_id].c_str());
			}
		}
	}
	return 0;
}

