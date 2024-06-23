#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <vector>
#include <algorithm>

#include <terark/util/profiling.hpp>
#include <terark/c/algorithm.h>

using namespace std;

struct Compare_second
{
	template<class P>
	bool operator()(const P& x, const P& y) const
	{
		return x.second < y.second;
	}
};

// for qsort
int compare_pp_second(const void* a, const void* b)
{
	std::pair<long long,long long>
		*x = (std::pair<long long,long long>*)a,
		*y = (std::pair<long long,long long>*)b;

	if (x->second < y->second)
		return -1;
	else if	(y->second < x->second)
		return 1;
	else
		return 0;
}

template<class T>
void check_v1_v2(const vector<T>& v1, const vector<T>& v2, const char* fname)
{
	// don't use v1 != v2, only compare x.second
	bool ok = true;
	for (int i = 0, n = v1.size(); i != n; ++i)
	{
		if (v1[i].second != v2[i].second)
		{
			ok = false;
			break;
		}
	}
	if (!ok)
	{
		fprintf(stderr, "%s error\n", fname);
	}
}

template<class T>
void check_res12(const vector<T>& v1, const vector<T>& v2, const char* func)
{
	for (int i=0,n=v1.size(); i<n; ++i)
	{
		pair<int,int> x0 = v1[i], x1 = v2[i];
		if (x0 != x1)
			fprintf(stderr, "%s error:%05d [(%d,%d)(%d,%d)]\n"
					, func, i
					, x0.first, x0.second
					, x1.first, x1.second
					);
	}
}

template<class T> void test_equal_range(const vector<T>& v)
{
	terark::profiling pf;

	int times = v.size() / 2;
	vector<pair<int,int> > f0, f1, f3;
	vector<long long> kv;
	f1.reserve(times);
	f0.reserve(times);
	f3.reserve(times);
	long long t0 = pf.now();
	for (int i = 0; i < times; ++i)
	{
		kv.push_back(rand() % v.size());
	}
	for (int i = 0; i < times; ++i)
	{
		int low1, low2, upp1, upp2;
		low1 = terark_equal_range_field(&v[0], v.size(), second, &upp1, kv[i]);
		low2 = terark_lower_bound_field(&v[0], v.size(), second, kv[i]);
		upp2 = terark_upper_bound_field(&v[0], v.size(), second, kv[i]);
		f1.push_back(make_pair(low1, upp1));
		f3.push_back(make_pair(low2, upp2));
	}
	long long t1 = pf.now();
	for (int i = 0; i < times; ++i)
	{
		T k = make_pair(0, kv[i]);
		const T* first = &v[0];
		const T* last = first + v.size();
		pair<const T*, const T*>
		ii = std::equal_range(first, last, k, Compare_second());
		// call lower/upper only...
		std::lower_bound(first, last, k, Compare_second());
		std::upper_bound(first, last, k, Compare_second());
		f0.push_back(pair<int,int>(ii.first-first, ii.second-first));
	}
	long long t2 = pf.now();

	check_res12(f0, f1, "terark_equal_range");
	check_res12(f0, f3, "terark_xxxer_bound");
	printf("vector.size=%d, timing: equal_range[terark=%lld, std=%lld, terark/std=%f]\n"
		, (int)v.size(), pf.ns(t0,t1), pf.ns(t1,t2), double(t1-t0)/(t2-t1)
		);
}

template<class T> void test_sort(const vector<T>& v)
{
	terark::profiling pf;

	vector<T> v1 = v, v2 = v, v3 = v;
	long long t0 = pf.now();
	terark_sort_field(&v1[0], v1.size(), second);
	long long t1 = pf.now();
	std::sort(&v2[0], &v2[0] + v2.size(), Compare_second());
	long long t2 = pf.now();
	qsort(&v3[0], v3.size(), sizeof(v3[0]), &compare_pp_second);
	long long t3 = pf.now();

	printf("vector.size=%d, timing: sort_______[terark=%lld, std=%lld, qsort=%lld, terark/std=%f]\n"
		, (int)v1.size(), pf.ns(t0,t1), pf.ns(t1,t2), pf.ns(t2,t3), double(t1-t0)/(t2-t1)
		);
	check_v1_v2(v1, v2, "terark_sort");
	test_equal_range(v1);
}

template<class T> void test_heap_sort(const vector<T>& v)
{
	terark::profiling pf;

	vector<T> v1 = v, v2 = v;

	long long t00 = pf.now();
	terark_make_heap_field(&v1[0], v1.size(), second);
	long long t01 = pf.now();

	long long t10 = pf.now();
	std::make_heap(&v2[0], &v2[0] + v2.size(), Compare_second());
	long long t11 = pf.now();

	check_v1_v2(v1, v2, "terark_make_heap");

	long long t02 = pf.now();
	terark_sort_heap_field(&v1[0], v1.size(), second);
	long long t03 = pf.now();

	long long t12 = pf.now();
	std::sort_heap(&v2[0], &v2[0] + v2.size(), Compare_second());
	long long t13 = pf.now();

	check_v1_v2(v1, v2, "terark_sort_heap");

	printf("vector.size=%d, timing: make_heap__[terark=%lld, std=%lld, terark/std=%f]\n"
		, (int)v1.size(), pf.ns(t00,t01), pf.ns(t10,t11), (t01-t00)/double(t11-t10)
		);
	printf("vector.size=%d, timing: sort_heap__[terark=%lld, std=%lld, terark/std=%f]\n"
		, (int)v1.size(), pf.ns(t02,t03), pf.ns(t12,t13), (t03-t02)/double(t13-t12)
		);
}

void test_arraycb()
{
	terark_arraycb acb;
	for (int i = 0; i < tev_type_count__; ++i)
	{
		terark_arraycb_init(&acb, TERARK_C_MAX_VALUE_SIZE, 8, (field_type_t)i, 0);
		terark_arraycb_init(&acb, TERARK_C_MAX_VALUE_SIZE, 8, (field_type_t)i, 1);
	}
}

int main(int argc, char* argv[])
{
	int size = argc >= 2 ? atoi(argv[1]) : 1000;

	terark::profiling pf;
	srand((int)pf.now());
	vector<std::pair<long long,long long> > v1;
	v1.resize(size);

	for (int i = 0; i != (int)v1.size(); ++i)
	{
		v1[i] = make_pair(i, rand() % v1.size() + 1);
	}

	test_heap_sort(v1);
	test_sort(v1);

	return 0;
}

struct A { int x, y; }; // x, y can be all base type: char/float/double/ptr etc...
struct Compare_A_x // only needed by std algorithm, maybe cause code explosion
{
	bool operator()(const A& x, const A& y) const { return x.x < y.x; }
	bool operator()(const A* x, const A* y) const { return x->x < y->x; }
};

void demo_foo(std::vector<A>& va, std::vector<A*>& vpa)
{
	// terark_sort_xxx is macro, called a C function, will not cause code explosion
	// more fast 15% than std::sort
	terark_sort_field(&*va.begin(), va.size(), x); // in cpp, auto deduce type of x
	terark_sort_field_c(&*va.begin(), va.size(), x, tev_int); // in C, can not deduce type of x
	std::sort(va.begin(), va.end(), Compare_A_x());

	terark_sort_field_p(&*vpa.begin(), vpa.size(), x); // in cpp, auto deduce type of x
	terark_sort_field_pc(&*vpa.begin(), vpa.size(), x, tev_int); // in C, can not deduce type of x
	std::sort(vpa.begin(), vpa.end(), Compare_A_x());
}
