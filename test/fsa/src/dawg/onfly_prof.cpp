#include <terark/fsa/dawg.hpp>
#include <terark/fsa/onfly_minimize.hpp>
#include <terark/hash_strmap.hpp>
#include <terark/util/profiling.hpp>
#include <stdio.h>
#include <iostream>

using namespace terark;

typedef Automata<State5B> au_t;

template<class OnflyType, class StrMap>
void test(const StrMap& word_cnt, const char* name) {
	au_t d1;
  {
	OnflyType onfly(&d1);
	for (size_t i = 0; i < word_cnt.end_i(); ++i) {
		onfly.add_word(word_cnt.key(i));
	}
  }
    au_t d2;
	d2.path_zip(d1, "DFS");
	long words = d1.dag_pathes();
	printf("%s: words=%ld states[used=%zd free=%zd] mem_size=%zd, path_ziped: states[used=%zd free=%zd] mem=%zd\n"
		, name, words
		, d1.num_used_states(), d1.num_free_states(), d1.mem_size()
		, d2.num_used_states(), d2.num_free_states(), d2.mem_size()
		);
}

int main(int argc, char* argv[]) {
	terark::profiling pf;
	hash_strmap<long> word_cnt;
	std::string line;
	long lineno = 0;
	long long t0 = pf.now();
	while (std::getline(std::cin, line)) {
		while (!line.empty() && isspace(line.back()))
			 line.resize(line.size()-1);
		if (!line.empty())
			word_cnt[line]++;
		if (++lineno % 100000 == 0)
			printf("lineno=%09ld len=%zd\n", lineno, line.size());
	}
	long long t1 = pf.now();
	word_cnt.sort_fast();
	long long t2 = pf.now();
	test<MinADFA_OnflyBuilder<au_t>         >(word_cnt, "onfly1");
	long long t3 = pf.now();
	test<MinADFA_OnflyBuilder<au_t>::Ordered>(word_cnt, "onfly2");
	long long t4 = pf.now();
	printf("read_file=%f hash_sort=%f's onfly1=%f's onfly2=%f's\n"
		, pf.sf(t0,t1), pf.sf(t1,t2), pf.sf(t2,t3), pf.sf(t3,t4));

	return 0;
}

