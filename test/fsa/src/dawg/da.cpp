#include <terark/fsa/dawg.hpp>
#include <terark/fsa/onfly_minimize.hpp>
#include <terark/util/profiling.hpp>
#include <terark/util/linebuf.hpp>
#include <terark/hash_strmap.hpp>
#include <terark/util/autoclose.hpp>
#include <iostream>
#include "dawg_patch.hpp"

using namespace terark;

template<class Au1, class Au2, class Au3, class Au4>
void print_trans_num(size_t raw_size, const Au1& au1, const Au2& au2, const Au3& au3, const Au4& au4) {
	valvec<int> tr1(256, 0);
	valvec<int> tr2(256, 0);
	valvec<int> tr3(256, 0);
	valvec<int> tr4(256, 0);
	size_t nt1 = 0;
	size_t nt2 = 0;
	size_t nt3 = 0;
	size_t nt4 = 0;
	for(size_t s = 0; s < au1.total_states(); ++s) {
		size_t n = au1.num_children(s);
		tr1[n]++; nt1 += n;
	}
	for(size_t s = 0; s < au2.total_states(); ++s) {
		size_t n = au2.num_children(s);
		tr2[n]++; nt2 += n;
	}
	for(size_t s = 0; s < au3.total_states(); ++s) {
		size_t n = au3.num_children(s);
		tr3[n]++; nt3 += n;
	}
	for(size_t s = 0; s < au4.total_states(); ++s) {
		size_t n = au4.num_children(s);
		tr4[n]++; nt4 += n;
	}
	double rs = raw_size;
	double b1 = au1.mem_size();
	double b2 = au2.mem_size();
	double b3 = au3.mem_size();
	double b4 = au4.mem_size();
	double s1 = au1.total_states();
	double s2 = au2.total_states();
	double s3 = au3.total_states();
	double s4 = au4.total_states();
	double t1 = nt1;
	double t2 = nt2;
	double t3 = nt3;
	double t4 = nt4;
	printf("=======================================================================\n");
	printf("RawInputSize=%zu\n", raw_size);
	printf("-----------------------------------------------------------------------\n");
	printf("|             |     trie    |     dawg    |     pzip    |     zzip    |\n");
	printf("-----------------------------------------------------------------------\n");
	printf("| total_state | %11zu | %11zu | %11zu | %11zu |\n", au1.total_states(), au2.total_states(), au3.total_states(), au4.total_states());
	printf("| total_trans | %11zu | %11zu | %11zu | %11zu |\n", nt1, nt2, nt3, nt4);
	printf("| total_bytes | %11zu | %11zu | %11zu | %11zu |\n", au1.mem_size(), au2.mem_size(), au3.mem_size(), au4.mem_size());
	printf("-----------------------------------------------------------------------\n");
	printf("| trans/state | %11.7f | %11.7f | %11.7f | %11.7f |\n", t1/s1, t2/s2, t3/s3, t4/s4);
	printf("| bytes/state | %11.7f | %11.7f | %11.7f | %11.7f |\n", b1/s1, b2/s2, b3/s3, b4/s4);
	printf("-----------------------------------------------------------------------\n");
	printf("| state(i/1)  | %11.7f | %11.7f | %11.7f | %11.7f |\n", s1/s1, s2/s1, s3/s1, s4/s1);
	printf("| state(1/i)  | %11.7f | %11.7f | %11.7f | %11.7f |\n", s1/s1, s1/s2, s1/s3, s1/s4);
	printf("-----------------------------------------------------------------------\n");
	printf("| trans(i/1)  | %11.7f | %11.7f | %11.7f | %11.7f |\n", t1/t1, t2/t1, t3/t1, t4/t1);
	printf("| trans(1/i)  | %11.7f | %11.7f | %11.7f | %11.7f |\n", t1/t1, t1/t2, t1/t3, t1/t4);
	printf("-----------------------------------------------------------------------\n");
	printf("| bytes(i/1)  | %11.7f | %11.7f | %11.7f | %11.7f |\n", b1/b1, b2/b1, b3/b1, b4/b1);
	printf("| bytes(1/i)  | %11.7f | %11.7f | %11.7f | %11.7f |\n", b1/b1, b1/b2, b1/b3, b1/b4);
	printf("-----------------------------------------------------------------------\n");
	printf("|rawsize(i/r) | %11.7f | %11.7f | %11.7f | %11.7f |\n", b1/rs, b2/rs, b3/rs, b4/rs);
	printf("|rawsize(r/i) | %11.7f | %11.7f | %11.7f | %11.7f |\n", rs/b1, rs/b2, rs/b3, rs/b4);
	printf("-----------------------------------------------------------------------\n");
	printf("-------------------------------------------------------------------------------\n");
	printf("|trans|         trie          |         dawg          |         pzip          |\n");
	printf("|trans|  states   |   ratio   |  states   |   ratio   |  states   |   ratio   |\n");
	printf("-------------------------------------------------------------------------------\n");
 	for (int i = 0; i < 256; ++i) {
		if (tr1[i] == 0 && tr2[i] == 0 && tr3[i] == 0) continue;
		printf("| %3d | %9d | %9.7f | %9d | %9.7f | %9d | %9.7f |\n", i
				, tr1[i], tr1[i]/s1
				, tr2[i], tr2[i]/s2
				, tr3[i], tr3[i]/s3
			  );
	}
	printf("-------------------------------------------------------------------------------\n");
}

template<class Dict>
void test_query_per_second(const char* name, const Dict& dict, const hash_strmap<unsigned>& mwc) {
    terark::profiling pf;
	long long t0 = pf.now();
	const int loop = 3;
	for (size_t i = 0; i < mwc.size(); ++i) {
		fstring w = mwc.key(i);
		size_t idx;
		for (int j = 0; j < loop; ++j)
			idx = dict.index(w);
		if (idx != i)
			printf("--%4zd %4zd  %s\n", idx, i, w.p);
		assert(idx == i);
	}
	long long t1 = pf.now();
	double nano = pf.nf(t0, t1) + 0.1; // avoid divide by zero
	double size = mwc.size() * loop + 0.1;
	printf("%s: query-per-second: %11.3f, nano-seconds-per-query: %11.6f\n"
			, name, 1e9*size/nano, nano/size);
}

int main(int argc, char** argv) {
    using namespace std;

	long long t0, t1, t2, t3;
	MyDAWG<State32> dawg2;
    MyDAWG<State32> trie;
    MyDAWG<State5B> dmin, zdfs, zbfs, zpfs;
	MinADFA_OnflyBuilder<MyDAWG<State32> > onfly(&dawg2);
	Automata<State5B> pzip, zzip;
	dmin = zdfs;
    terark::profiling pf;
	hash_strmap<unsigned> mwc;
	long hsize = 0;
    {
        //std::map<string, unsigned> mwc;
        unsigned lineno = 0;
		terark::LineBuf line;
        for (; line.getline(stdin) >= 0; ++lineno) {
			line.trim();
			if (line.n) {
				trie.add_word(line);
				mwc[line]++;
			}
        }
		hsize = mwc.size();
		printf("total lines=%u\n", lineno);
		printf("mwc.sort\n");
		t0 = pf.now();
		mwc.sort_fast();
		t1 = pf.now();
		printf("hash_strmap sort time: %lf\n", pf.sf(t0, t1));
    }
    printf("non-minimized, mem=%ld, free=%ld\n", (long)trie.mem_size(), (long)trie.frag_size());
//	trie.print_output();
    if (argc >= 2) trie.write_dot_file(argv[1]);
    trie.compile();
	size_t twb = 0; // total word bytes
	long const max_test = 8*1000*1000;
	for (size_t i = 0; i < mwc.size(); ++i) twb += mwc.key(i).n;
	printf("average key length = %f\n", (double)twb/mwc.size());
	if (hsize < max_test)
		test_query_per_second("trie", trie, mwc);
	else
		mwc.clear();
//  trie.print_output();
    t0 = pf.now();
//  trie.graph_dfa_minimize_opt();
    trie.trie_dfa_minimize(dmin);
    t1 = pf.now();
    printf("has-minimized, mem=%ld, free=%ld, time=%lf's\n"
			, (long)dmin.mem_size()
			, (long)dmin.frag_size(), pf.sf(t0, t1)
			);
    if (argc >= 3) dmin.write_dot_file(argv[2]);
    long cnt = dmin.dag_pathes();
    printf("dag_pathes=%ld, words=%ld\n", (long)cnt, hsize);
    assert(cnt == hsize);
    if (argc >= 3) dmin.write_dot_file(argv[2]);
    dmin.compile();
    if (argc >= 3) dmin.write_dot_file_ex(argv[2]);
   	t0 = pf.now();
	pzip.path_zip(dmin, "DFS");
	t1 = pf.now();
    printf("path_zipped, zpath_states=%ld, non_dup_pool: mem=%ld, free=%ld, time=%lf's\n"
			, (long)pzip.num_zpath_states()
			, (long)pzip.mem_size()
			, (long)pzip.frag_size(), pf.sf(t0, t1)
			);
	t0 = pf.now();
	zzip.path_zip(dmin, "DFS");
	t1 = pf.now();
    printf("path_zipped, zpath_states=%ld, has_dup_pool: mem=%ld, free=%ld, time=%lf's\n"
			, (long)zzip.num_zpath_states()
			, (long)zzip.mem_size()
			, (long)zzip.frag_size(), pf.sf(t0, t1)
			);
	if (hsize < max_test) {
		t0 = pf.now();
		zdfs.path_zip(dmin, "DFS"); zdfs.compile();
		t1 = pf.now();
		zbfs.path_zip(dmin, "BFS"); zbfs.compile();
		t2 = pf.now();
		zpfs.path_zip(dmin, "PFS"); zpfs.compile();
		t3 = pf.now();
		printf("path_zip_time[DFS=%f BFS=%f PFS=%f]\n", pf.sf(t0,t1), pf.sf(t1,t2), pf.sf(t2,t3));
	#if !defined(NDEBUG)
		zdfs.tpl_for_each_word([&](size_t i, fstring w, size_t){assert(mwc.key(i) == w);});
		zbfs.tpl_for_each_word([&](size_t i, fstring w, size_t){assert(mwc.key(i) == w);});
		zpfs.tpl_for_each_word([&](size_t i, fstring w, size_t){assert(mwc.key(i) == w);});
	#endif
		test_query_per_second("dawg", dmin, mwc);
		test_query_per_second("zDFS", zdfs, mwc);
		test_query_per_second("zBFS", zbfs, mwc);
		test_query_per_second("zPFS", zpfs, mwc);
		assert(dmin.dag_pathes() == zdfs.dag_pathes());
		assert(dmin.dag_pathes() == zbfs.dag_pathes());
		assert(dmin.dag_pathes() == zpfs.dag_pathes());
	}
	t0 = pf.now();
	for (size_t i = 0; i < mwc.size(); ++i) {
		fstring w = mwc.key(i);
		onfly.add_word(w);
	}
	t1 = pf.now();
	if (dawg2.total_states() < 1000)
		dawg2.write_dot_file("onfly.dot");
	else
		printf("skip generating onfly.dot\n");
	if (mwc.empty()) {
		printf("skip check onfly\n");
	} else {
		long cnt2 = dawg2.dag_pathes();
		printf("dag_path[onfly=%ld, dmin=%ld], state_num[onfly=%ld, dmin=%ld], onfly.free_states=%ld\n"
				, (long)cnt2, (long)cnt
				, (long)dawg2.num_used_states(), (long)dmin.total_states()
				, (long)dawg2.num_free_states()
				);
		printf("insert-per-second: %f, nano-seconds-per-insert: %f, time=%f's\n"
				, cnt2/pf.sf(t0,t1), pf.nf(t0,t1)/cnt2, pf.sf(t0,t1));
		fflush(stdout);
		assert(cnt2 == cnt);
	}
//	if (mwc.size() < 8*1024) trie.print_output();

	print_trans_num(twb, trie, dmin, pzip, zzip);
#ifndef NDEBUG
	valvec<unsigned> rev_tsort;
	ptrdiff_t i = 0, j = 0;
	dmin.topological_sort0(0, [&](unsigned s){ rev_tsort.push_back(s); i++;});
	dmin.topological_sort2(0, [&](unsigned s){ assert(rev_tsort[j] == s); j++;});
	assert(i == j);
#endif
#if 0
	terark::Auto_fclose f0(fopen("tsort0.txt", "w"));
	terark::Auto_fclose f2(fopen("tsort2.txt", "w"));
	dmin.topological_sort0(0, [&](unsigned s){ fprintf(f0, "%u\n", s);});
	dmin.topological_sort2(0, [&](unsigned s){ fprintf(f2, "%u\n", s);});
#endif
    return 0;
}


