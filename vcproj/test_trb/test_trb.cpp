// test_trb.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <limits>
#include <map>
#include <terark/util/profiling.hpp>
#include <terark/c/algorithm.h>
#include <terark/c/trb.h>

#undef NDEBUG
#include <assert.h>

#if defined(_WIN32) || defined(_WIN64)
# define WIN32_LEAN_AND_MEAN
# define NOMINMAX
# include <Windows.h>
#endif

#undef max

#define TRACE_THIS_LINE0()    printf("%s:%d\n", __FILE__, __LINE__)
#define TRACE_THIS_LINE1(msg) printf("%s:%d %s\n", __FILE__, __LINE__, msg)

using namespace std;

template<class KeyType>
void
check_inbound(const trb_vtab& vtab, const trb_tree& m1, std::map<KeyType, unsigned>& m0, int cnt)
{
	typedef std::map<KeyType, unsigned> m0_t;
	typedef typename m0_t::value_type value_type;

	printf("%s check find inbound...", (char*)vtab.app_data);  fflush(stdout);
	// check find/lower_bound/upper_bound/equal_range inbound
	int nth = 0;
	for (typename m0_t::const_iterator iter = m0.begin(); iter != m0.end(); ++iter)
	{
		if (cnt-1 == nth)
			nth = nth;
		struct trb_node* node0 = trb_find_tev_app(&vtab, &m1, &iter->first);
		struct trb_node* node1 = vtab.pf_find(&vtab, &m1, &iter->first);
		struct trb_node* node2 = vtab.pf_lower_bound(&vtab, &m1, &iter->first);
		struct trb_node* node3 = vtab.pf_upper_bound(&vtab, &m1, &iter->first);
		struct trb_node* node2e, *node3e;
		node2e = vtab.pf_equal_range(&vtab, &m1, &iter->first, &node3e);
		typename m0_t::const_iterator iter2 = m0.lower_bound(iter->first);
		typename m0_t::const_iterator iter3 = m0.upper_bound(iter->first);
		value_type v2 = trb_data_ref(&vtab, node2, value_type);
		value_type v3 = node3 ? trb_data_ref(&vtab, node3, value_type) : v2;
		assert(*iter2 == v2);
		assert(0 == node3 || *iter3 == v3);
		assert(NULL != node0);
		assert(NULL != node1);
		assert(node0 == node1);
		assert(node2e == node2);
		assert(node3e == node3);
		assert(*iter == trb_data_ref(&vtab, node0, value_type));
		++nth;
	}
	printf("ok\n");
}

template<class KeyType>
void
check_onbound(const trb_vtab& vtab, const trb_tree& m1, std::map<KeyType, unsigned>& m0, int cnt)
{
	typedef std::map<KeyType, unsigned> m0_t;
	typedef typename m0_t::value_type value_type;

	printf("%s check find onbound...", (char*)vtab.app_data);  fflush(stdout);
	// check find/lower_bound/upper_bound/equal_range on bound
	{
		KeyType lmax = std::numeric_limits<KeyType>::max();//cnt + 1;
		KeyType lmin = -1;
		struct trb_node* node2 = vtab.pf_lower_bound(&vtab, &m1, &lmax);
		struct trb_node* node3 = vtab.pf_upper_bound(&vtab, &m1, &lmax);
		struct trb_node* node2e, *node3e;
		node2e = vtab.pf_equal_range(&vtab, &m1, &lmax, &node3e);
		assert(NULL == node2 || trb_data_ref(&vtab, node2, value_type).first == lmax);
		assert(NULL == node3);
		assert(node2e == node2);
		assert(node3e == node3);
		node2 = vtab.pf_lower_bound(&vtab, &m1, &lmin);
		node3 = vtab.pf_upper_bound(&vtab, &m1, &lmin);
		node2e = vtab.pf_equal_range(&vtab, &m1, &lmin, &node3e);
		assert(NULL != node2);
		assert(NULL != node3);
		assert(node2 == node3);
		assert(node2e == node2);
		assert(node3e == node3);
	}
	printf("ok\n");
}

template<class KeyType>
void test_trb()
{
	printf("infun:%s:##############################################\n", BOOST_CURRENT_FUNCTION);
	terark::profiling pf;

	typedef std::map<KeyType, unsigned> m0_t;
	typedef typename m0_t::value_type value_type;
	m0_t m0;
	int cnt = 32*1024;
	int nth = 0;
	const int node_offset = 2*sizeof(void*);
	const int data_offset = node_offset + sizeof(trb_node);

	struct trb_node* upp00;
	trb_vtab vfast = {0};
	vfast.data_size = sizeof(value_type);
	vfast.key_offset = offsetof(value_type, first);
	vfast.node_offset = node_offset;
	vfast.data_offset = data_offset;
	trb_vtab_init(&vfast, terark_get_field_type((KeyType*)0));
	vfast.app_data = (void*)"fast";

	trb_vtab vslow = {0};
	vslow.pf_compare = vfast.pf_compare; // use slow
	vslow.data_size = sizeof(value_type);
	vslow.key_offset = offsetof(value_type, first);
	vslow.node_offset = node_offset;
	vslow.data_offset = data_offset;
	trb_vtab_init(&vslow, terark_get_field_type((KeyType*)0));
	vslow.app_data = (void*)"slow";

	trb_tree m1 = {0};
	TRACE_THIS_LINE1("generate data...");
	for (int i = 0; i < cnt; ++i)
	{
	//	printf("i=%d\n", i);
		KeyType rd = rand() % cnt;
		pair<typename m0_t::iterator, bool> ib = m0.insert(value_type(rd, 0));
		ib.first->second++;

		typename m0_t::key_type lli = rd;
		int existed;
		struct trb_node* node = trb_probe(&vfast, &m1, &lli, &existed);
		assert(NULL != node);
		assert(lli == rd);
		assert(!existed == ib.second);
		if (existed) {
			trb_data_ref(&vfast, node, value_type).second++;
			const value_type v1 = trb_data_ref(&vfast, node, value_type);
			assert(v1 == *ib.first);
		} else {
			trb_data_ref(&vfast, node, value_type).second=1;
			const value_type v1 = trb_data_ref(&vfast, node, value_type);
//			printf("key[m0=%06d, m1=%06d], val[m0=%d, m1=%u]\n", (int)ib.first->first, (int)v1.first, ib.first->second, v1.second);
			assert(v1 == *ib.first);
		}
	}
	{
		TRACE_THIS_LINE1("check trb_tree == std::map");
		assert(m0.size() == m1.trb_count);
		typename m0_t::iterator iter = m0.begin();
		TRACE_THIS_LINE0();
		struct trb_node* node = trb_iter_first(node_offset, &m1);
		TRACE_THIS_LINE0();
		int i;
		for (i = 0; iter != m0.end(); ++i, ++iter, node = trb_iter_next(node_offset, node))
		{
		//	printf("i=%d\n", i);
			value_type v0 = *iter;
			value_type v1 = trb_data_ref(&vfast, node, value_type);
			assert(v0 == v1);
		}
		assert(NULL == node);
	}
	printf("check find non-existed key...");  fflush(stdout);
	{
		typename m0_t::key_type key = 0x1172;
		typename m0_t::iterator iter0 = m0.find(key);
		bool existed = iter0 != m0.end();
		struct trb_node* iter1 = vfast.pf_find(&vfast, &m1, &key);
		assert(existed == (NULL != iter1));
	}
	printf("check delete...");  fflush(stdout);
	for (int i = 0; i < cnt/2; ++i)
	{
		KeyType rd = rand() % cnt;
		typename m0_t::key_type lli = rd;
		int existed = trb_erase(&vfast, &m1, &lli);
		int nerased = m0.erase(lli);
		assert(!existed == !nerased);
	}
	assert(m0.size() == m1.trb_count);
	printf("ok, m1.trb_count=%d\n", (int)m1.trb_count);
	check_inbound(vfast, m1, m0, cnt);
	check_inbound(vslow, m1, m0, cnt);
	check_onbound(vfast, m1, m0, cnt);
	check_onbound(vslow, m1, m0, cnt);
	printf("check iteration forward...");  fflush(stdout);
	{
		struct trb_node* iter1 = trb_iter_first(node_offset, &m1);
		typename m0_t::iterator iter0 = m0.begin();
		for (; iter0 != m0.end(); )
		{
			assert(*iter0 == trb_data_ref(&vfast, iter1, value_type));
			++iter0;
			iter1 = trb_iter_next(node_offset, iter1);
		}
		assert(NULL == iter1);
	}
	printf("ok\n");
	printf("check iteration backward...");  fflush(stdout);
	{
		struct trb_node* iter1 = trb_iter_last(node_offset, &m1);
		typename m0_t::reverse_iterator iter0 = m0.rbegin();
		for (; iter0 != m0.rend(); )
		{
			assert(*iter0 == trb_data_ref(&vfast, iter1, value_type));
			++iter0;
			iter1 = trb_iter_prev(node_offset, iter1);
		}
		assert(NULL == iter1);
	}

	printf("ok\n");
	printf("bench marking...");  fflush(stdout);

	int nloop = TERARK_IF_DEBUG(5, 100);
	long long t00 = pf.now();
	for (int i = 0; i < nloop; ++i)
		for (typename m0_t::const_iterator iter = m0.begin(); iter != m0.end(); ++iter)
			vslow.pf_find(&vslow, &m1, &iter->first);
	long long t01 = pf.now();
	for (int i = 0; i < nloop; ++i)
		for (typename m0_t::const_iterator iter = m0.begin(); iter != m0.end(); ++iter)
			vfast.pf_find(&vfast, &m1, &iter->first);
	long long t02 = pf.now();
	for (int i = 0; i < nloop; ++i)
		for (typename m0_t::const_iterator iter = m0.begin(); iter != m0.end(); ++iter)
			m0.find(iter->first);

	long long t10 = pf.now();
	for (int i = 0; i < nloop; ++i)
		for (typename m0_t::const_iterator iter = m0.begin(); iter != m0.end(); ++iter)
			vslow.pf_lower_bound(&vslow, &m1, &iter->first);
	long long t11 = pf.now();
	for (int i = 0; i < nloop; ++i)
		for (typename m0_t::const_iterator iter = m0.begin(); iter != m0.end(); ++iter)
			vfast.pf_lower_bound(&vfast, &m1, &iter->first);
	long long t12 = pf.now();
	for (int i = 0; i < nloop; ++i)
		for (typename m0_t::const_iterator iter = m0.begin(); iter != m0.end(); ++iter)
			m0.lower_bound(iter->first);

	long long t20 = pf.now();
	for (int i = 0; i < nloop; ++i)
		for (typename m0_t::const_iterator iter = m0.begin(); iter != m0.end(); ++iter)
			vslow.pf_equal_range(&vslow, &m1, &iter->first, &upp00);
	long long t21 = pf.now();
	for (int i = 0; i < nloop; ++i)
		for (typename m0_t::const_iterator iter = m0.begin(); iter != m0.end(); ++iter)
			vfast.pf_equal_range(&vfast, &m1, &iter->first, &upp00);
	long long t22 = pf.now();
	for (int i = 0; i < nloop; ++i)
		for (typename m0_t::const_iterator iter = m0.begin(); iter != m0.end(); ++iter)
			m0.equal_range(iter->first);

	double cc = m0.size() * nloop;
	double cn = m0.size();

	long long t30 = pf.now();
	for (int i = 0; i < nloop; ++i)
	{
		struct trb_node* iter1 = trb_iter_first(node_offset, &m1);
		for (; iter1; )
			iter1 = trb_iter_next(node_offset, iter1);
		assert(NULL == iter1);
	}
	long long t31 = pf.now();
	for (int i = 0; i < nloop; ++i)
	{
		typename m0_t::iterator iter0 = m0.begin();
		for (; iter0 != m0.end(); )
			++iter0;
	}
	long long t32 = pf.now();

	for (int i = 0, n=cc; i < n; ++i)
	{
		KeyType x = i + 100, y = i & 0xFFF;
		int cmp = vslow.pf_compare(&vslow, &m1, &x, &y);
	}
	long long t33 = pf.now();

	for (int i = 0, n=cc; i < n; ++i)
	{
		volatile KeyType x = i + 100, y = i & 0xFFF;
	}
	long long t34 = pf.now();

	int su = 0;
	for (int i = 0, n=cc; i < n; ++i)
	{
		KeyType x = i + 100, y = i & 0xFFF;
		if (x < y)
			su += 1;
		else if (y < x)
			su += 2;
		else
			su += 3;
	}
	long long t35 = pf.now();

	while (m1.trb_count)
		trb_erase(&vfast, &m1, &trb_data_ref(&vfast, trb_iter_first(node_offset, &m1), value_type).first);

	long long t40 = pf.now();

	while (!m0.empty())
		m0.erase(m0.begin()->first);
	long long t41 = pf.now();

	printf("ok\n");
	printf("___find____[slow=%lld:%7.3f, fast=%lld:%7.3f, stl=%lld:%7.3f, fast/stl=%5.3f]\n", pf.us(t00,t01), pf.ns(t00,t01)/cc, pf.us(t01,t02), pf.ns(t01,t02)/cc, pf.us(t02,t10), pf.ns(t02,t10)/cc, (t02-t01)/double(t10-t02));
	printf("lower_bound[slow=%lld:%7.3f, fast=%lld:%7.3f, stl=%lld:%7.3f, fast/stl=%5.3f]\n", pf.us(t10,t11), pf.ns(t10,t11)/cc, pf.us(t11,t12), pf.ns(t11,t12)/cc, pf.us(t12,t20), pf.ns(t12,t20)/cc, (t12-t11)/double(t20-t12));
	printf("equal_range[slow=%lld:%7.3f, fast=%lld:%7.3f, stl=%lld:%7.3f, fast/stl=%5.3f]\n", pf.us(t20,t21), pf.ns(t20,t21)/cc, pf.us(t21,t22), pf.ns(t21,t22)/cc, pf.us(t22,t30), pf.ns(t22,t30)/cc, (t22-t21)/double(t30-t22));
	printf("comp_by_fun[doit=%lld:%7.3f, null=%lld:%7.3f, pse=%lld:%7.3f]\n", pf.us(t32,t33), pf.ns(t32,t33)/cc, pf.us(t33,t34), pf.ns(t33,t34)/cc, pf.us(t34,t35), pf.ns(t34,t35)/cc);
	printf("iteration__[slow=N/A,  fast=%lld:%7.3f, stl=%lld:%7.3f]\n", pf.us(t30,t31), pf.ns(t30,t31)/cc, pf.us(t31,t32), pf.ns(t31,t32)/cc);
	printf("delonebyone[slow=N/A,  fast=%lld:%7.3f, stl=%lld:%7.3f]\n", pf.us(t35,t40), pf.ns(t35,t40)/cn, pf.us(t40,t41), pf.ns(t40,t41)/cn);
	printf("su=%d\n", su); // only for disable su optimization
#if defined(_WIN32) || defined(_WIN64)
	LARGE_INTEGER freq;
	QueryPerformanceFrequency(&freq);
	printf("cpu freq is %llu\n", freq.QuadPart);
#endif
	trb_destroy(&vfast, &m1);
}

void test_trb_cxx();

int main(int argc, char* argv[])
{

	test_trb<short>();

	test_trb<int>();

	test_trb<long long>();

	test_trb_cxx();

	return 0;
}
