// test_trb.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <map>
#include <vector>
#include <algorithm>
#include <typeinfo>

#include <terark/c/algorithm.h>

#include <terark/io/MemStream.hpp>
#include <terark/io/DataIO.hpp>
#include <terark/trb_cxx.hpp>
#include <terark/stdtypes.hpp>
#include <terark/util/profiling.hpp>

using namespace std;
using namespace terark;
using terark::trbmap;
using terark::trbset;
using terark::trbtab;

struct var_data
{
	long cnt;
	long key;
	long nlen;
	char data[1];

	void filldata(size_t dsize)
	{
		cnt = 0;
		key = rand() % 256;
		nlen = dsize - offsetof(var_data, data);
		sprintf(data, "key=%08d", (int)key);
	}

	static size_t get_data_size(const struct trb_vtab* vtab, const void* data)
	{
		const var_data* d = (const var_data*)data;
		return offsetof(var_data, data) + d->nlen;
	}

	static int trb_comp_key(const struct trb_vtab* vtab,
							const struct trb_tree* tree,
							const void* x,
							const void* y)
	{
		assert(NULL != vtab);
		assert(NULL != x);
		assert(NULL != y);
		long lx = *(long*)x, ly = *(long*)y;
		if (lx < ly) return -1;
		if (lx > ly) return +1;
		return 0;
	}

	static int trb_comp_ptr(const struct trb_vtab* vtab,
							const struct trb_tree* tree,
							const void* x,
							const void* y)
	{
		assert(NULL != vtab);
		assert(NULL != x);
		assert(NULL != y);
		const var_data* px = *(const var_data**)x, *py = *(const var_data**)y;

		if (px->key < py->key) return -1;
		if (px->key > py->key) return +1;
		return 0;
	}
};

#if 0
void test_trb_cxx_var_data()
{
	printf("test_trb_cxx_var_data----------------------------------\n");

	typedef trbtab<long, var_data, offsetof(var_data, key), var_data::trb_comp_key> tab_t;
	typedef trbset<const var_data*, var_data::trb_comp_ptr> ptab_t;
	tab_t::s_vtab.pf_data_get_size = &var_data::get_data_size;
	tab_t::s_vtab.pf_data_copy = NULL; // let it memcpy the data
	tab_t tab;
	ptab_t ptab;
	map<long, long> lmap;
	for (int i = 0; i < TERARK_IF_DEBUG(5000, 10000); ++i)
	{
		size_t dsize = 32 + rand() % 128;
		trb_node* node1 = tab.alloc_node(dsize);
		tab.get_data(node1)->filldata(dsize);
		trb_node* node2= tab.insert_node(node1);
		if (node2 != node1)
		{// existed
			tab.free_node(node1, dsize);
		}
		ptab.insert(tab.get_data(node2));
		tab.get_data(node2)->cnt++;
		lmap[tab.get_data(node2)->key]++;
	}

	for (map<long,long>::iterator iter = lmap.begin(); lmap.end() != iter; ++iter)
	{
		{
			tab_t::iterator j = tab.find(iter->first);
			assert(!j.is_null());
			assert(iter->second == j->cnt);
		}
		{
			var_data tmp; tmp.key = iter->first;
			ptab_t::iterator j = ptab.find(&tmp);
			assert(!j.is_null());
			assert(iter->second == (*j)->cnt);
		}
	}

	tab.clear();
	printf("test_trb_cxx_var_data ok!\n");
}
#endif

std::vector<std::string> gen_rand_strs(int count)
{
	std::vector<std::string> vs;
	for (int i = 0; i < count; ++i)
	{
		char szbuf[512];
		int r = rand();
		int n = sprintf(szbuf, "%08d-", r);
		int r2 = r % 257;
		for (int j = 0; j < r2; ++j)
			szbuf[n+j] = "0123456789ABCDEF"[j % 16];
		szbuf[n+r2] = 0;
		vs.push_back(szbuf);
	}
	return vs;
}

const char* hexnum(long x)
{
	static char buf[20];
	int i=0;
	for (; x; ++i)
	{
		buf[i] = "0123456789ABCDEF"[x&15];
		x = (unsigned long)(x) >> 4;
	}
	for (int j=0; j < i/2; ++j)
	{
		char t = buf[j];
		buf[j] = buf[i-j];
		buf[i-j] = t;
	}
	buf[i] = 0;
	return buf;
}
template<class SMT>
void save_and_load(SMT& m)
{
	size_t size1 = m.size();
	PortableDataOutput<AutoGrownMemIO> out;
	out << m;
	m.clear();
	assert(m.size() == 0);
	PortableDataInput<MemIO> in; in.set(out.buf(), out.tell());
	in >> m;
	assert(m.size() == size1);
}

template<class SMT>
void test_smap()
{
//	printf("map_t=%s\n", typeid(SMT).name());
	printf("infun:%s\n", BOOST_CURRENT_FUNCTION);
	SMT smap;
	smap["1"] = 0;
	smap["2"] = 1;
	smap["3"] = 1;
	smap["4"] = 2;
	smap["1"]++;
	smap["4"] += 2;
	smap.insert("5", 5);
//	smap.insert("5", 6);
	assert(!smap.insert("5", 6).second);

	smap["aabbccddeeffgghhiijjkk========================================================"] = 10;
	assert(smap.lower_bound("3") == smap.lower_bound("3", 2));
	assert(smap.upper_bound("3") == smap.upper_bound("3", 2));
	assert(smap.equal_range("3") == smap.equal_range("3", 2));

	assert(smap.key(smap.lower_bound("3", 2))[0] == '3');
	assert(smap.equal_range("3", 2).first != smap.equal_range("3", 2).second);
	assert(smap.equal_range("3", 2).first == smap.lower_bound("3", 2));
	assert(smap.equal_range("3", 2).second== smap.upper_bound("3", 2));
	assert(smap.key(smap.equal_range("3", 2).first)[0] == '3');
	size_t erased = smap.erase("3");
	assert(erased == 1);

	for (typename SMT::iterator i = smap.begin(); i != smap.end(); ++i)
	{
		printf("smap[%s]=%d, klen=%d\n", smap.key(i), (int)*i, (int)smap.klen(i));
	}

	int cnt = TERARK_IF_DEBUG(10000, 100000);
	typedef std::map<std::string
		, typename SMT::mapped_type
		, std::less<std::string>
		, typename SMT::allocator_type
	> stdmap_t;
	stdmap_t stdm;
	SMT sm2;

	srand(1235);
	std::vector<std::string> v1 = gen_rand_strs(cnt)
//		, v2(cnt) // v2 保留第一个字节，以便在 smap 查找时和内部cstr对齐
		;

//	for (int i=0; i<cnt; ++i)
//		v2[i].append("1"), v2[i].append(v1[i]);
//	std::random_shuffle(v2.begin(), v2.end());

	terark::profiling pf;
	long long t0 = pf.now();
	for (int i=0; i<cnt; ++i) stdm[v1[i]]++;
	long long t1 = pf.now();

	for (int i=0; i<cnt; ++i) {
		bool toolarge = false;
	   	try {
			sm2[v1[i]]++;
		}
		catch (const std::invalid_argument& ) {
			toolarge = true;
		}
		if (v1[i].size() + 1 > sm2.keylenmax) {
			assert(toolarge);
		}
	}
	long long t2 = pf.now();
	std::random_shuffle(v1.begin(), v1.end());
	long long tt = pf.now();

	for (int i=0; i<cnt; ++i) stdm[v1[i]]++;
	long long t3 = pf.now();

	for (int i=0; i<cnt; ++i) {
		bool toolarge = false;
	   	try {
			sm2[v1[i].c_str()]++;
		}
		catch (const std::invalid_argument& ) {
			toolarge = true;
		}
		if (v1[i].size() + 1 > sm2.keylenmax) {
			assert(toolarge);
		}
	}
	long long t4 = pf.now();

	printf("insert[std=%lld, my=%lld, std/my=%f]\n", pf.us(t0,t1), pf.us(t1,t2), (double)pf.us(t0,t1)/pf.us(t1,t2));

	// cnt 较小时，std更快
	// cnt 变大时，my 更快，这是因为 cache miss, my 消耗的内存小，访问内部 str 是直接访问，不需要间接
	printf("find__[std=%lld, my=%lld, std/my=%f]\n", pf.us(tt,t3), pf.us(t3,t4), (double)pf.us(tt,t3)/pf.us(t3,t4));

	SMT smap2 = smap;
	for (int loop = 0; loop < 2; ++loop)
	{
		// loop 0: test copy correction
		// loop 1: test save/load correction
		typename SMT::iterator iter = smap.begin(), jter = smap2.begin();
		size_t nr = 0;
		for (; iter != smap.end(); ++iter, ++jter, ++nr)
		{
			assert(iter.get_node() != jter.get_node());
			size_t kl1 = smap.klen(iter);
			size_t kl2 = smap2.klen(jter);
			const char *k1 = smap.key(iter), *k2 = smap2.key(jter);
			int v1 = *iter, v2 = *jter;
			assert(v1 == v2);
			assert(kl1 == kl2);
			assert(memcmp(k1, k2, kl1) == 0);
		}
		assert(iter == smap.end());
		assert(jter == smap2.end());
		save_and_load(smap2);
	}
}

void test_trb_cxx()
{
// test trbstrmap
//	typedef terark::trbstrmap<int, &trb_compare_tev_inline_cstr_nocase> smap_t;
	enum {	NodeOffset1 = 2*sizeof(void*), DataOffset1 = NodeOffset1 + sizeof(trb_node),
		NodeOffset2 = 4*sizeof(void*), DataOffset2 = NodeOffset2 + sizeof(trb_node),
	};
	test_smap<terark::trbstrmap<int, trb_compare_tev_inline_cstr, std::allocator <int>, NodeOffset1, DataOffset1> >();
	test_smap<terark::trbstrmap<int, trb_compare_tev_inline_cstr, terark::mpoolxx<int>, NodeOffset2, DataOffset2> >();
	test_smap<terark::trbstrmap<int, trb_compare_tev_inline_cstr_nocase, terark::mpoolxx<int> > >();
	test_smap<terark::trbstrmap2<int, trb_compare_tev_inline_cstr_nocase, terark::mpoolxx<int> > >();

	terark::profiling pf;

//	test_trb_cxx_var_data();

	printf("infun:%s:#################################################\n", BOOST_CURRENT_FUNCTION);
	typedef std::map<long long, unsigned> m0_t;
	typedef trbmap<long long, unsigned> m1_t;
	m0_t m0;
	int cnt = 32*1024;
	int nth = 0;
	m1_t m1;
	printf("generate data...");  fflush(stdout);
	for (int i = 0; i < cnt; ++i)
	{
	//	printf("i=%d\n", i);
		long long rd = rand() % cnt;
		m0[rd]++;
		m1[rd]++;
	}
	assert(m0.size() == m1.size());
	printf("ok.\n");
	printf("check find non-existed key...");  fflush(stdout);
	{
		m0_t::key_type key = 0x1172;
		m0_t::iterator iter0 = m0.find(key);
		m1_t::iterator iter1 = m1.find(key);
		assert((m0.end() == iter0) == (m1.end() == iter1));
	}
	printf("ok.\n");
	printf("check delete...");  fflush(stdout);
	for (int i = 0; i < cnt/2; ++i)
	{
		long long rd = rand() % cnt;
		m0_t::key_type lli = rd;
		int nerased0 = m0.erase(lli);
		int nerased1 = m1.erase(lli);
		assert(nerased0 == nerased1);
	}
	assert(m0.size() == m1.size());
	printf("ok, m1.trb_count=%d\n", (int)m1.size());
	printf("check find inbound...");  fflush(stdout);
	// check find/lower_bound/upper_bound/equal_range inbound
	nth = 0;
	for (m0_t::const_iterator iter = m0.begin(); iter != m0.end(); ++iter)
	{
		if (cnt-1 == nth)
			nth = nth;
		m1_t::iterator node1 = m1.find(iter->first);
		m1_t::iterator node2 = m1.lower_bound(iter->first);
		m1_t::iterator node3 = m1.upper_bound(iter->first);
		pair<m1_t::iterator,m1_t::iterator> ii = m1.equal_range(iter->first);
		m0_t::const_iterator iter2 = m0.lower_bound(iter->first);
		m0_t::const_iterator iter3 = m0.upper_bound(iter->first);
		assert(*iter2 == *node2);
		assert(m1.end() == node3 || *iter3 == *node3);
		assert(!node1.is_null());
		assert(ii.first  == node2);
		assert(ii.second == node3);
		assert(*iter == *node1);
		++nth;
	}
	printf("ok\n");
	printf("check find onbound...");  fflush(stdout);
	// check find/lower_bound/upper_bound/equal_range on bound
	{
		long long lmax = cnt + 1;
		long long lmin = -1;
		m1_t::iterator node2 = m1.lower_bound(lmax);
		m1_t::iterator node3 = m1.upper_bound(lmax);
		pair<m1_t::iterator,m1_t::iterator> ii = m1.equal_range(lmax);
		assert(node2.is_null());
		assert(node3.is_null());
		assert(ii.first.is_null());
		assert(ii.second.is_null());
		node2 = m1.lower_bound(lmin);
		node3 = m1.upper_bound(lmin);
		ii = m1.equal_range(lmin);
		assert(!node2.is_null());
		assert(!node3.is_null());
		assert(node2 == node3);
		assert(ii.first  == node2);
		assert(ii.second == node3);
	}
	printf("ok\n");
	printf("check iteration forward...");  fflush(stdout);
	{
		m0_t::iterator iter0 = m0.begin();
		m1_t::iterator iter1 = m1.begin();
		for (; iter0 != m0.end(); ++iter0, ++iter1)
		{
			assert(*iter0 == *iter1);
		}
		assert(iter1.is_null());
	}
	printf("ok\n");
	printf("check iteration backward...");  fflush(stdout);
	{
		m0_t::reverse_iterator iter0 = m0.rbegin();
		m1_t::reverse_iterator iter1 = m1.rbegin();
		for (; iter0 != m0.rend(); ++iter0, ++iter1)
		{
			assert(*iter0 == *iter1);
		}
		assert(iter1.is_null());
	}
	{
		m1_t m12(m1);
		save_and_load(m12);
	}
	printf("ok\n");
	printf("bench marking...");  fflush(stdout);

	int nloop = TERARK_IF_DEBUG(5, 100);
	long long t0 = pf.now();
	for (int i = 0; i < nloop; ++i)
		for (m0_t::const_iterator iter = m0.begin(); iter != m0.end(); ++iter)
			trb_find_tev_app(&m1_t::s_vtab, m1.get_trb_tree(), &iter->first);
	long long t1 = pf.now();
	for (int i = 0; i < nloop; ++i)
		for (m0_t::const_iterator iter = m0.begin(); iter != m0.end(); ++iter)
			m1.find(iter->first);
	long long t2 = pf.now();
	for (int i = 0; i < nloop; ++i)
		for (m0_t::const_iterator iter = m0.begin(); iter != m0.end(); ++iter)
			m0.find(iter->first);

	long long t3 = pf.now();
	for (int i = 0; i < nloop; ++i)
		for (m0_t::const_iterator iter = m0.begin(); iter != m0.end(); ++iter)
			m1.lower_bound(iter->first);
	long long t4 = pf.now();
	for (int i = 0; i < nloop; ++i)
		for (m0_t::const_iterator iter = m0.begin(); iter != m0.end(); ++iter)
			m0.lower_bound(iter->first);

	long long t5 = pf.now();
	for (int i = 0; i < nloop; ++i)
		for (m0_t::const_iterator iter = m0.begin(); iter != m0.end(); ++iter)
			m1.equal_range(iter->first);
	long long t6 = pf.now();
	for (int i = 0; i < nloop; ++i)
		for (m0_t::const_iterator iter = m0.begin(); iter != m0.end(); ++iter)
			m0.equal_range(iter->first);
	long long t7 = pf.now();
	for (int i = 0; i < nloop; ++i)
	{
		m1_t::iterator iter1 = m1.begin();
		for (; iter1; ++iter1)
			NULL;
		assert(iter1.is_null());
	}
	long long t8 = pf.now();
	for (int i = 0; i < nloop; ++i)
	{
		m0_t::iterator iter0 = m0.begin();
		for (; iter0 != m0.end(); )
			++iter0;
	}
	long long t9 = pf.now();

	while (!m1.empty())
		m1.erase(m1.begin());

	long long t10 = pf.now();

	while (!m0.empty())
		m0.erase(m0.begin()->first);
	long long t11 = pf.now();

	printf("ok\n");
	printf("___find____[slow=%lld, fast=%lld, stl=%lld, fast/stl=%f]\n", pf.us(t0,t1), pf.us(t1,t2), pf.us(t2,t3), (double)pf.us(t1,t2)/pf.us(t2,t3));
	printf("lower_bound[fast=%lld, stl=%lld, fast/stl=%f]\n", pf.us(t3,t4), pf.us(t4,t5), (double)pf.us(t3,t4)/pf.us(t4,t5));
	printf("equal_range[fast=%lld, stl=%lld, fast/stl=%f]\n", pf.us(t5,t6), pf.us(t6,t7), (double)pf.us(t5,t6)/pf.us(t6,t7));
	printf("iteration__[fast=%lld, stl=%lld, fast/stl=%f]\n", pf.us(t7,t8), pf.us(t8,t9), (double)pf.us(t7,t8)/pf.us(t8,t9));
	printf("delonebyone[fast=%lld, stl=%lld, fast/stl=%f]\n", pf.us(t9,t10), pf.us(t10,t11), (double)pf.us(t9,t10)/pf.us(t10,t11));
}

