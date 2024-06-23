#include <boost/regex/pending/unicode_iterator.hpp>
#include <string.h>
#include <stdio.h>
#include <vector>

void split(const char* delim, char* str, std::vector<char*>* F) {
	char* next = str;
	char* curr;
	F->resize(0);
	while ((curr = strsep(&next, delim)) != NULL) {
		F->push_back(curr);
	}
}

int main(int argc, char* argv[]) {
	size_t len1 = 0;
	ssize_t len2 = 0;
	char* line = NULL;
	std::vector<char*> F;
	while ((len2 = getline(&line, &len1, stdin)) > 0) {
		while (len2 > 0 && isspace(line[len2-1])) line[--len2] = 0;
		split(" \t", line, &F);
		if (F.size() >= 3 && strcmp(F[1], "kXHC1983") == 0 && F[0][0] == 'U' && F[0][1] == '+') {
			typedef boost::u32_to_u8_iterator<const unsigned*> u32iter;
			unsigned uc32 = strtoul(F[0]+2, NULL, 16);
			char utf8[16];
			std::copy(u32iter(&uc32), u32iter(&uc32+1), utf8)[0] = '\0';
			for (size_t i = 2; i < F.size(); ++i) {
				const char* py = strchr(F[i], ':');
				if (NULL == py)
					fprintf(stderr, "Data Format Error\n");
				else
					printf("%s\t%s\n", utf8, py+1);
			}
		}
	}
	return 0;
}

