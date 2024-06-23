#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#define _SCL_SECURE_NO_WARNINGS
#endif
#include <terark/zsrch/fcgi_searcher.hpp>

void usage(const char* prog) {
fprintf(stderr, R"EOS(
Usage: %s Options RegexQuery
Options:
   -d InvertIndexDirectory
   -a
      Show all doc/log content
   -h
      Show this help info
)EOS", prog);
}

int main(int argc, char* argv[])
try {
	const char* indexDir = "index";
#if !defined(_MSC_VER)
	setenv("DFA_MAP_LOCKED", "1", false);
	setenv("DFA_MAP_POPULATE", "1", false);
#endif
	for (;;) {
		int opt = getopt(argc, argv, "d:h");
		switch (opt) {
		default:
		case '?':
			usage(argv[0]);
			return 1;
		case -1:
			goto GetoptDone;
		case 'd':
			indexDir = optarg;
			break;
		case 'h':
			usage(argv[0]);
			return 0;
		}
	}
GetoptDone:
	if (NULL == indexDir) {
		fprintf(stderr, "-d InvertIndexDirectory is missing\n");
		usage(argv[0]);
		return 1;
	}
	IndexReaderPtr indexReader(IndexReader::loadIndex(indexDir));
	fprintf(stderr, "%s: loaded index: %s\n", argv[0], indexDir);
	return FCGI_ExecuteSearcherLoop(indexReader.get());
}
catch (const std::exception& ex) {
	return 3;
}

