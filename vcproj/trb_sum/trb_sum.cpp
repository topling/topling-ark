// trb_sum.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <terark/trb_cxx.hpp>
#include <algorithm>
#include <vector>

struct Data
{
	int key;
	int count;
	int vsum;
	int val;

	explicit Data(int i)
	{
		key = i;
		count = 1;
		val = i;
		vsum = val;
	}
	Data()
	{
		key = 0;
		count = 0;
		vsum = 0;
		val = 0;
	}

	static void add(void* pdest, const void* psrc)
	{
		Data* dest = (Data*)pdest;
		const Data* src = (const Data*)psrc;

		dest->count += src->count;
		dest->vsum += src->vsum;
	}
	static void sub(void* pdest, const void* psrc)
	{
		Data* dest = (Data*)pdest;
		const Data* src = (const Data*)psrc;

		dest->count -= src->count;
		dest->vsum -= src->vsum;
	}
	static void copy(void* pdest, const void* psrc)
	{
		Data* dest = (Data*)pdest;
		const Data* src = (const Data*)psrc;

//		dest->val = src->val;
		dest->count = src->count;
		dest->vsum = src->vsum;
	}
	static void init(void* pdest)
	{
		Data* dest = (Data*)pdest;
//		assert(dest->val >= 0);
		dest->count = 1;
		dest->vsum = dest->val;
	}
	static void atom_add(void* pdest, const void* px)
	{
		Data* dest = (Data*)pdest;
		const Data* x = (const Data*)px;
		dest->count += 1;
		dest->vsum += x->val;
	}
	static void atom_sub(void* pdest, const void* px)
	{
		Data* dest = (Data*)pdest;
		const Data* x = (const Data*)px;
		assert(dest->count >=2);
		dest->count -= 1;
		dest->vsum -= x->val;
	}
	static int sum_comp(const void* px, const void* py)
	{
		const Data* x = (const Data*)px;
		const Data* y = (const Data*)py;
	//	assert(x->count == y->count);
		if (x->count != y->count)
		{
			x = x;
		}
		return x->count - y->count;
	}
};

class CMain : public terark::trbtab<int, Data>
{
public:
	void insert_some(int loop)
	{
		printf("insert from min to max...\n");
		for (int i = 0; i < loop/3; ++i)
		{
			if (i == 2)
				i = i;
			Data d(i);
			this->insert(d);
		}

		printf("insert from max to min...\n");
		for (int i = 2*loop/3 - 1; i >= loop/3; --i)
		{
			Data d(i);
			this->insert(d);
		}

		printf("insert randomly...\n");
		std::vector<int> ra(loop/3);
		for (int i = 0, n = ra.size(); i < n; ++i)
			ra[i] = i + 2*loop/3;
		std::random_shuffle(ra.begin(), ra.end());
		for (int i = 0; i < loop/3; ++i)
		{
			Data d(ra[i]);
			this->insert(d);
		}
	}

	void remove_min_max(int loop)
	{
		printf("remove from min to max...\n");
		for (int i = 0; i < loop/3; ++i)
		{
			this->erase(i);
		}
	}
	void remove_max_min(int loop)
	{
		printf("remove from max to min...\n");
		for (int i = 2*loop/3 - 1; i >= loop/3; --i)
		{
			this->erase(i);
		}
	}
	void remove_random(int loop)
	{
		printf("remove randomly...\n");
		std::vector<int> ra(loop/3);
		for (int i = 0, n = ra.size(); i < n; ++i)
			ra[i] = i + 2*loop/3;
		std::random_shuffle(ra.begin(), ra.end());
		for (int i = 0; i < loop/3; ++i)
		{
			if (9 == i)
				i = i;
			this->erase(ra[i]);
		}
	}

	void test_seq(int loop)
	{
		int found = 0;
		for (int i = 0; i < loop; ++i)
		{
			Data s;	s.key = rand() % loop;
			iterator iter = s_vtab.pf_get_sum(&s_vtab, &m_tree, &s.key, &s);
			if (iter != end()) {
				Data f = *iter;
				assert(f.key == s.key);
				int y1 = 0;
				int rank = 0;
				for (iterator j = begin(); j != iter; ++j) {
					Data jd = *j;
					assert(jd.key == rank);
					y1 += j->key;
					rank++;
				}
				int x2 = f.key*(f.key-1)/2;
				assert(y1 == x2);
				assert(s.count == rank); // count is rank
				assert(s.count == f.key);
				assert(s.vsum == x2);
				++found;
			}
		}
		printf("loop=%d, trb_get_sum: found=%d, not found=%d\n", loop, found, 3*loop - found);
		printf("trb_sum Test passed!\n");
	}

	void test_rand(int loop)
	{
		int found = 0;
		for (int i = 0; i < loop; ++i)
		{
			Data s;	s.key = rand() % loop;
			iterator iter = s_vtab.pf_get_sum(&s_vtab, &m_tree, &s.key, &s);
			if (iter != end()) {
				Data f = *iter;
				int sum = 0;
				int rank = 0;
				for (iterator j = begin(); j != iter; ++j) {
					sum += j->key;
					rank++;
				}
				assert(s.count == rank);
				assert(s.vsum == sum);
				++found;
			}
		}
		printf("loop=%d, trb_get_sum: found=%d, not found=%d\n", loop, found, 3*loop - found);
		printf("trb_sum Test passed!\n");
	}

	int main(int argc, char* argv[])
	{
		trb_vtab_init(&s_vtab, s_vtab.key_type);

		s_vtab.pf_sum_add = &Data::add;
		s_vtab.pf_sum_sub = &Data::sub;
		s_vtab.pf_sum_copy = &Data::copy;
		s_vtab.pf_sum_init = &Data::init;
		s_vtab.pf_sum_atom_add = &Data::atom_add;
		s_vtab.pf_sum_atom_sub = &Data::atom_sub;
		s_vtab.pf_sum_comp = &Data::sum_comp;

		srand(1234567);

		const int loop = 1500;
		insert_some(loop);
		test_seq(loop);
		remove_random(loop);
		test_rand(loop);
		remove_max_min(loop);
		test_rand(loop);
		return 0;
	}
};


int main(int argc, char* argv[])
{
	CMain m;
	int ret = m.main(argc, argv);
	return ret;
}

