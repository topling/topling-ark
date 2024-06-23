#include <terark/hash_strmap.hpp>
#include <fstream>
#include <string.h>
#include <stdio.h>
#include <errno.h>

int main(int argc, char** argv) {
	hash_strmap<unsigned> wc;
	for (int i = 1; i < argc; ++i) {
		std::ifstream ifs(argv[i]);
		if (!ifs.is_open()) {
			fprintf(stderr, "can not open file: %s\n", argv[i]);
			continue;
		}
		std::string line;
		long lineno = 0;
		while (!ifs.eof() && std::getline(ifs, line)) {
			while (line.size() && isspace(line.end()[-1]))
				line.resize(line.size()-1);
			if (line.empty())
				continue;
			size_t cntpos = line.find('\t');
			if (0 == cntpos) {
				continue; // empty word
			}
			int cnt = 1;
			if (line.npos != cntpos) {
				cnt = atoi(line.c_str() + cntpos);
				cnt = std::max(1, cnt);
			} else {
				cntpos = line.size();
			}
			fstring word(line.c_str(), cntpos);
			wc[word] += cnt;
			++lineno;
		}
	}
	for (size_t i = 0; i < wc.end_i(); ++i) {
		printf("%s\t%u\n", wc.key(i).p, wc.val(i));
	}
	return 0;
}

