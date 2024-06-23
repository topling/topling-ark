// test_mpool.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <typeinfo>
#include <vector>
#include <map>
#include <list>

#include <terark/stdtypes.hpp>
#include <terark/c/mpool.h>
#include <terark/mpoolxx.hpp>

#include <terark/util/profiling.hpp>

void test_fixed_mpool()
{
	printf("testing fixed_mpool...\n");
	struct fixed_mpool fmp = { 0 };
	fmp.cell_size = 8;
	fmp.chunk_size = 125;
	fixed_mpool_init(&fmp);
	std::vector<void*> vp;
	for (int i = 0; i < 1000000; ++i)
	{
		if (vp.empty() || rand() % 7 < 4)
			vp.push_back(fixed_mpool_alloc(&fmp));
		else {
			int j = i % vp.size();
			if (vp[j]) {
				fixed_mpool_free(&fmp, vp[j]);
				vp[j] = NULL;
			} else {
				vp.push_back(fixed_mpool_alloc(&fmp));
			}
		}
	}

	printf("used=%zd\n", fmp.used_cells);

	fixed_mpool_destroy(&fmp);
}

void test_mpool()
{
	using namespace std;
	printf("testing mpool...\n");
	struct mpool mp = { 0 };
	mp.chunk_size = 4096;
	mp.max_cell_size = 256;
	mp.big_flags = 0
		|TERARK_MPOOL_ALLOW_BIG_BLOCK
		|TERARK_MPOOL_AUTO_FREE_BIG
//		|TERARK_MPOOL_MALLOC_BIG
		;
	mpool_init(&mp);
	vector<pair<void*, size_t> > vp;
	for (int i = 0; i < 100000; ++i)
	{
		if (vp.empty() || rand() % 7 < 4)
			vp.push_back(make_pair(mpool_salloc(&mp, i % 512 +80), i % 512 +80));
		else {
			int j = i % vp.size();
			if (vp[j].first) {
				mpool_sfree(&mp, vp[j].first, vp[j].second);
				vp[j].first = NULL;
			} else {
				vp.push_back(make_pair(mpool_salloc(&mp, i % 512 +5), i % 512 +5));
			}
		}
	}
	size_t small = 0;
	for (int i = 0; i != (int)vp.size(); ++i)
	{
		if (vp[i].first && vp[i].second <= mp.max_cell_size)
			small++;
	}
#ifdef _MSC_VER
#define FZD "%Id"
#else
#define FZD "%zd"
#endif

	printf("used[bytes="FZD", cells="FZD", small="FZD"], big_blocks="FZD", big_total_size="FZD"\n"
		, mpool_used_bytes(&mp), mpool_used_cells(&mp), small, mp.big_blocks, mp.big_list.size);

	mpool_destroy(&mp);
}

struct sample_data
{
//	int a, b, c, d;
	uintptr_t a;
};

void bench_fixed(int n)
{
	using namespace std;

	std::allocator<sample_data> stdal;
	struct fixed_mpool fmp = {0};
	fmp.cell_size = sizeof(sample_data);
	fmp.chunk_size = 8192;
	fmp.nChunks = 1024;
	fixed_mpool_init(&fmp);

	terark::profiling pf;

	vector<sample_data*> vp0(n), vp1(n);

	long long t0, t1, t2, t3, t4, t5, t6;
	t0 = pf.now();
	for (int i = 0; i+3 < n; i+=4)
	{
		vp0[i+0] = stdal.allocate(1);
		vp0[i+1] = stdal.allocate(1);
		vp0[i+2] = stdal.allocate(1);
		vp0[i+3] = stdal.allocate(1);
	}
	t1 = pf.now();
	for (int i = 0; i+3 < n; i+=4)
	{
		vp1[i+0] = (sample_data*)fixed_mpool_alloc(&fmp);
		vp1[i+1] = (sample_data*)fixed_mpool_alloc(&fmp);
		vp1[i+2] = (sample_data*)fixed_mpool_alloc(&fmp);
		vp1[i+3] = (sample_data*)fixed_mpool_alloc(&fmp);
	}
	t2 = pf.now();
	for (int i = 0; i+3 < n; i+=4)
	{
		stdal.deallocate(vp0[i+0], sizeof(sample_data));
		stdal.deallocate(vp0[i+1], sizeof(sample_data));
		stdal.deallocate(vp0[i+2], sizeof(sample_data));
		stdal.deallocate(vp0[i+3], sizeof(sample_data));
	}
	t3 = pf.now();
	for (int i = 0; i+3 < n; i+=4)
	{
		fixed_mpool_free(&fmp, vp1[i+0]);
		fixed_mpool_free(&fmp, vp1[i+1]);
		fixed_mpool_free(&fmp, vp1[i+2]);
		fixed_mpool_free(&fmp, vp1[i+3]);
	}
	t4 = pf.now();
	for (int i = 0; i < n; ++i)
	{
		stdal.deallocate(stdal.allocate(1), sizeof(sample_data));
	}
	t5 = pf.now();
	for (int i = 0; i < n; ++i)
	{
		fixed_mpool_free(&fmp, fixed_mpool_alloc(&fmp));
	}
	t6 = pf.now();

	printf("alloc[std=%lld, my=%lld, std/my=%f], free[std=%lld, my=%lld, std/my=%f], alloc+free[std=%lld, my=%lld, std/my=%f]\n"
		, pf.us(t0,t1), pf.us(t1,t2), (double)pf.us(t0,t1)/pf.us(t1,t2)
		, pf.us(t2,t3), pf.us(t3,t4), (double)pf.us(t2,t3)/pf.us(t3,t4)
		, pf.us(t4,t5), pf.us(t5,t6), (double)pf.us(t4,t5)/pf.us(t5,t6)
		);

	fixed_mpool_destroy(&fmp);
}

template<class Alloc>
void test_alloc(int count)
{
	terark::profiling pf;
//	typedef std::map<int, int, std::less<int>, Alloc> map_t;
	std::list<int, Alloc> li;
	long long t0 = pf.now();
//	map_t m1;
	for (int i = 0; i < count; ++i)
		li.push_back(i);
	long long t1 = pf.now();
	printf("%s: %lld\n", typeid(Alloc).name(), pf.us(t0, t1));
}

int main(int argc, char* argv[])
{
//	test_fixed_mpool();
	test_mpool();
	bench_fixed(1000000);

	int count = TERARK_IF_DEBUG(2000, 100000);
	test_alloc<terark::fixed_mpoolxx<int> >(count);
	test_alloc<terark::mpoolxx<int> >(count);
	test_alloc<std::allocator<int> >(count);
	return 0;
}

