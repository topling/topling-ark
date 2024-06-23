// sufsort.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <terark/valvec.hpp>
#include <terark/fsa/automata.hpp>
#include <terark/util/linebuf.hpp>
#include <terark/util/profiling.hpp>
#include <terark/fsa/suffix_array_trie.hpp>

using namespace terark;
int main(int argc, char* argv[]) {
	SortableStrVec  strVec;
	SuffixTrieCacheDFA suf;
	profiling pf;
	LineBuf line;
	while (line.getline(stdin) > 0) {
		line.chomp();
		strVec.push_back(line);
	}
	size_t minTokenLen =  8;
	size_t minFreq     = 32;
	size_t maxDepth    = 16;
	llong t0 = pf.now();
	suf.build_sa(strVec);
	llong t1 = pf.now();
	printf("build_sa: %zd bytes, time: %f seconds, through-put: %f MB/s\n"
		, suf.m_sa.size(), pf.sf(t0,t1), suf.m_sa.size()/pf.uf(t0,t1)
		);
	suf.bfs_build_cache(minTokenLen, minFreq, maxDepth);
	llong t2 = pf.now();
	printf("bfs_build_cache: time: %f seconds\n", pf.sf(t1,t2));
	suf.sa_print_stat();

//	suf.dfs_build_cache(minTokenLen, minFreq, maxDepth);
	llong t3 = pf.now();
	printf("dfs_build_cache: time: %f seconds\n", pf.sf(t2,t3));
	suf.sa_print_stat();

	for (size_t i = 1; i < argc; ++i) {
		const byte* key = (byte*)argv[i];
		printf("--------------------------------------------\n");
		printf("key: %s\n", key);
		auto match = suf.sa_match_max_length(key, 1);
		suf.sa_print_match(match);
	}
    return 0;
}

