#include <terark/valvec.hpp>
#include <fstream>
#include <algorithm>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <limits.h>

struct Node {
	size_t   offset;
	unsigned len;
	unsigned freq;
};
struct GreaterFreq {
	template<class T>
	bool operator()(const T& x, const T& y) const {
		return x.freq > y.freq;
	}
};

int main(int argc, char** argv) {
	valvec<Node> link;
	valvec<char> pool;
	unsigned max_word_len = UINT_MAX;
	unsigned min_freq = 1;
	if (const char* env = getenv("max_word_len")) {
		max_word_len = atoi(env);
	}
	if (const char* env = getenv("min_freq")) {
		min_freq = std::max(atoi(env), 1);
	}
	for (int i = 1; i < argc; ++i) {
		std::ifstream ifs(argv[i]);
		if (!ifs.is_open()) {
			fprintf(stderr, "can not open file: %s\n", argv[i]);
			continue;
		}
		std::string line;
		long lineno = 0;
		while (!ifs.eof() && std::getline(ifs, line)) {
			++lineno;
			while (line.size() && isspace(line.end()[-1]))
				line.resize(line.size()-1);
			if (line.empty())
				continue;
			size_t tabpos = line.find('\t');
			if (0 == tabpos) {
				continue; // empty word
			}
			Node node;
			if (line.npos != tabpos) {
				node.freq = atoi(line.c_str() + tabpos + 1);
				node.freq = std::max(1u, node.freq);
			} else {
				node.freq = 1;
				tabpos = line.size();
			}
			if (node.freq >= min_freq && node.len <= max_word_len) {
				node.offset = pool.size();
				node.len = tabpos;
				link.push_back(node);
				pool.append(line.c_str(), tabpos);
			}
		}
	}
	std::sort(link.begin(), link.end(), GreaterFreq());
	for (size_t i = 0; i < link.size(); ++i) {
		Node ni = link[i];
		const char* word = &pool[ni.offset];
		printf("%.*s\t%u\n", ni.len, word, ni.freq);
	}
	return 0;
}

