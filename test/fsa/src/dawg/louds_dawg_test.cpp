#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#define _SCL_SECURE_NO_WARNINGS
#endif
#include <terark/fsa/nest_trie_dawg.hpp>
#include <getopt.h>
#include <stdio.h>

using namespace terark;

void usage(const char* prog) {
	fprintf(stderr, R"EOS(Usage: %s Options\n
		-m MaxVal
		-v : be verbose
	)EOS"
		, prog);
}

int main(int argc, char* argv[]) {
	bool verbose = false;
	long maxVal = 10000;
	for (;;) {
		int opt = getopt(argc, argv, "m:v");
		switch (opt) {
		default:
			usage(argv[0]);
			return 3;
		case -1:
			goto GetoptDone;
		case 'm':
			maxVal = strtol(optarg, NULL, 10);
			break;
		case 'v':
			verbose = true;
			break;
		}
	}
GetoptDone:
	SortableStrVec strVec;
	for(long i = 0; i < maxVal; ++i) {
		char buf[64];
		long len = sprintf(buf, "%012ld", i * 1000);
		strVec.push_back(fstring(buf, len));
	}
	NestLoudsTrieDAWG_SE_512 trie;
	NestLoudsTrieConfig conf;
	conf.initFromEnv();
	trie.build_from(strVec, conf);
	assert(trie.num_words() == (size_t)maxVal);
	for(long i = 0; i < maxVal; ++i) {
		char buf[64];
		long len = sprintf(buf, "%012ld", i * 1000);
		long j = trie.index(fstring(buf, len));
		std::string str = trie.nth_word(i);
		if (verbose && (j != i || fstring(buf, len) != str)) {
			printf("i: %8ld, j: %8ld, buf: %ld:%s, str: %ld:%s\n"
				, i, j, len, buf, (long)str.size(), str.c_str());
		}
		assert(str.size() == (size_t)len);
		assert(j == i);
		assert(fstring(buf, len) == str);
	}
	return 0;
}

