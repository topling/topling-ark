#include "suffix_tree.hpp"
#include <stddef.h>
#include <assert.h>
#include <stdio.h>
#include <algorithm>

namespace terark {

class suffix_array_impl
{
public:
	std::vector<int> sa, rank;

	struct Comp0
	{
		Comp0(const int * r) : rank(r) {}

		bool operator()(int x, int y) const { return rank[x] < rank[y]; }
		const int *rank;
	};

	struct CompFirstN
	{
		CompFirstN(const int* r, int FirstN)
		   	: rank(r), FirstN(FirstN) {}
/*
		int comp(int x, int y) const
		{
			const int *px = rank + x, *py = rank + y;
			for (int i = FirstN; i ; ++px, ++py, --i)
				if (*px != *py)
					return *px - *py;
			return 0;
		}
*/
		bool equal(int x, int y) const
		{
			const int *px = rank + x, *py = rank + y;
			for (int i = FirstN; i ; ++px, ++py, --i)
				if (*px != *py)
					return false;
			return true;
		}
		bool operator()(int x, int y) const
	   	{
			const int *px = rank + x, *py = rank + y;
			for (int i = FirstN; i ; ++px, ++py, --i)
				if (*px != *py)
					return *px < *py;
			return false;
	   	}
		const int *rank;
		int FirstN;
	};

	struct CompK
	{
		int k, n;
		const int *rank;

		bool equal(const int x, const int y) const
		{
			assert(x != y);
			assert(rank[x] == rank[y]);
			return // rank[x] == rank[y] &&
				x+k < n && y+k < n && rank[x+k] == rank[y+k];
		}
/*
		int comp(const int x, const int y) const
		{
		//	if (rank[x] < rank[y]) return -1;
		//	if (rank[x] > rank[y]) return +1;
		//	assert(x != y); // in std::sort, maybe x == y
			assert(x < n);
			assert(y < n);
			assert(rank[x] == rank[y]);
			if (x+k >= n || y+k >= n) {
				return y - x;
			//	printf("n=%ld, k=%ld, x=%u, rx=%u, y=%u, ry=%u]\n", (long)n, (long)k, x, rank[x], y, rank[y]);
			}
			return rank[x+k] - rank[y+k];
		}
*/
		bool operator()(const int x, const int y) const
		{
			assert(rank[x] == rank[y]);
			if (x+k >= n || y+k >= n) {
				return y < x;
			}
			return rank[x+k] < rank[y+k];
		}
	};

public:

	void append(int ch)
	{
		rank.push_back(ch + 1);
		sa.push_back((int)sa.size());
	}

	void build()
	{
		if (sa.empty()) return;

		// initially, let rank(i) as the CharAt(i)
		// rank(i) only assure:
		//   if suffix(i, k) <  suffix(j, k) then rank(i, k) <  rank(j, k)
		//   if suffix(i, k) == suffix(j, k) then rank(i, k) == rank(j, k)
		// when k==1, CharAt(i) is equivalent to rank(i,k=1)
		//
		std::sort(sa.begin(), sa.end(), Comp0(&rank[0]));

		CompK ck;
		ck.n = (int)sa.size();
		ck.k = 1; // initial k=1
		std::vector<int> rank2(rank.size()); // Rank-2k

		int distinct = 0;  // distinct string by CompK(k)
		for (; ck.k < ck.n && distinct != ck.n; ck.k *= 2)
		{
			ck.rank = &rank[0]; // `rank` swaped with `rank2`, update `ck.rank`

			distinct = 0;
			int distinct2 = 0; // distinct string by CompK(2k)

			for (int i = 0; i != ck.n; ++distinct)
			{
				const int low = i; // lower bound of current equal(k) range
				const int curr = rank[sa[i]]; // min rank value of current equal(k) range

				// find upper bound of current equal(k) range
				// sequential search is more fast than std::upper_bound
				do ++i;	while (i != ck.n && curr == rank[sa[i]]);

				// sort by CompK(2k)
				std::sort(sa.begin() + low, sa.begin() + i, ck);

				// split equal(k) range to equal(2k) range
				for (int j = low; ; ++distinct2) {
					int curr2 = sa[j]; // begin of equal(2k) range
					do {
						// `rank2[sa[j]]` is rank of suffix(j)
						rank2[sa[j]] = distinct2;
						if (++j == i)
							goto Done;
					} while (ck.equal(curr2, sa[j]));
				}
			Done:
				distinct2++;
			}
			rank.swap(rank2); // more fast than `rank = rank2;`
		//---------------------------------------------------------
		//	printf("rank[old=%4u, new=%4u], n=%ld, k=%4ld\n", distinct, distinct2, (long)ck.n, (long)ck.k);
			assert(distinct2 >= distinct);
		}
		return;
	}

	void build_n(int FirstN)
	{
		assert(FirstN >= 1);
		assert(FirstN <= (int)sa.size());
		assert(sa.size() == rank.size());

		if (sa.empty()) return;

		CompK ck;
		ck.n = (int)sa.size();
		ck.k = FirstN; // initial k=FirstN
		std::vector<int> rank2(rank.size()); // Rank-2k
		std::vector<int> addr(rank.size()+1); // [addr[i], addr[i+1]) is an equal(k) range
		std::vector<int> addr2(rank.size()+1); // for performance, define here

	//	addr[0] = 0; // not needed, default is 0

		int distinct = 0;  // distinct string by CompK(k)

		// initially, let rank(i) as the CharAt(i)
		// rank(i) only assure:
		//   if suffix(i, k) <  suffix(j, k) then rank(i, k) <  rank(j, k)
		//   if suffix(i, k) == suffix(j, k) then rank(i, k) == rank(j, k)
		// when k==1, CharAt(i) is equivalent to rank(i,k=1)
		//
		if (1 == FirstN) {
			std::sort(sa.begin(), sa.end(), Comp0(&rank[0]));
			for (int i = 0; i != ck.n; )
			{
				const int curr = rank[sa[i]]; // min rank value of current equal(k) range

				// find upper bound of current equal(k) range
				// sequential search is more fast than std::upper_bound
				do ++i;	while (i != ck.n && curr == rank[sa[i]]);

				addr[++distinct] = i;
			}
		} else {
			rank.resize(ck.n + FirstN - 1, 0); // append `FirstN - 1` chars with 0
			CompFirstN cfn(&rank[0], FirstN);
			std::sort(sa.begin(), sa.end(), cfn);

			for (int i = 0; i != ck.n; )
			{
				const int curr = sa[i];

				// find upper bound of current equal(k) range
				// sequential search is more fast than std::upper_bound
				do rank2[sa[i++]] = distinct; while (i != ck.n && cfn.equal(curr, sa[i]));

				addr[++distinct] = i;
			}
			rank.resize(ck.n);
			rank.swap(rank2);
		}

		for (; ck.k < ck.n && distinct != ck.n; ck.k *= 2)
		{
			ck.rank = &rank[0]; // `rank` swaped with `rank2`, update `ck.rank`
			int distinct2 = 0; // distinct string by CompK(2k)
		//	addr2[0] = 0; // not needed
			for (int i = 0; i != distinct; ++i)
			{
				const int low = addr[i]; // lower bound of current equal(k) range
				const int upp = addr[i+1];
				if (low + 1 != upp)
				{
				//	printf("i=%ld, low=%ld, upp=%ld\n", (long)i, (long)low, (long)upp);
					assert(low + 1 < upp);

					// sort by CompK(2k)
					std::sort(sa.begin() + low, sa.begin() + upp, ck);

					// split equal(k) range to equal(2k) range
					int j = low;
					do {
						int curr2 = sa[j]; // begin of equal(2k) range
						do {
							// `rank2[sa[j]]` is rank of suffix(j)
							rank2[sa[j]] = distinct2;
							++j;
						} while (j != upp && ck.equal(curr2, sa[j]));

						addr2[++distinct2] = j;

					} while (j != upp);
				}
			   	else // only 1, sort is not needed
			   	{
					rank2[sa[low]] = distinct2;
					addr2[++distinct2] = upp;
				}
			}
			// `swap` is faster than `operator=`
			addr.swap(addr2);
			rank.swap(rank2);
		//---------------------------------------------------------
		//	printf("rank[old=%4u, new=%4u], n=%ld, k=%4ld\n", distinct, distinct2, (long)ck.n, (long)ck.k);
			assert(distinct2 >= distinct);
			distinct = distinct2;
		}
		assert(distinct == ck.n);
	}

	// rank 保存 rank 而非排重后的名词
	// rank[sa[i]] == count of elements which `rank[sa[*]] < rank[sa[i]]`
	void build_o(int FirstN)
	{
		assert(FirstN >= 1);
		assert(FirstN <= (int)sa.size());
		assert(sa.size() == rank.size());

		if (sa.empty()) return;

		CompK ck;
		ck.n = (int)sa.size();
		ck.k = FirstN; // initial k=FirstN
		std::vector<int> rank2(rank.size()); // Rank-2k
		std::vector<int> eq_range, eq_range2;

		if (1 == FirstN) {
			std::sort(sa.begin(), sa.end(), Comp0(&rank[0]));
			for (int i = 0; i != ck.n; )
			{
				const int low = i;
				const int curr = rank[sa[i]]; // min rank value of current equal(k) range

				// find upper bound of current equal(k) range
				// sequential search is more fast than std::upper_bound
				do rank2[sa[i++]] = low; while (i != ck.n && curr == rank[sa[i]]);

				if (i - low > 1) {
					eq_range.push_back(low);
					eq_range.push_back(i);
				}
			}
		} else {
			rank.resize(ck.n + FirstN - 1, 0); // append `FirstN - 1` chars with 0
			CompFirstN cfn(&rank[0], FirstN);
			std::sort(sa.begin(), sa.end(), cfn);

			for (int i = 0; i != ck.n; )
			{
				const int low = i;
				const int curr = sa[i];

				// find upper bound of current equal(k) range
				// sequential search is more fast than std::upper_bound
				do rank2[sa[i++]] = low; while (i != ck.n && cfn.equal(curr, sa[i]));

				if (i - low > 1) {
					eq_range.push_back(low);
					eq_range.push_back(i);
				}
			}
			rank.resize(ck.n);
		}
		rank = rank2; // must be assign, not swap

		for (; ck.k < ck.n && !eq_range.empty(); ck.k *= 2)
		{
			assert(eq_range.size() % 2 == 0);
			eq_range2.resize(0);
			ck.rank = &rank[0]; // `rank` swaped with `rank2`, update `ck.rank`

			// 可并行, parallelable
			for (size_t i = 0; i != eq_range.size(); i += 2)
			{
				const int low = eq_range[i+0]; // lower bound of current equal(k) range
				const int upp = eq_range[i+1];
			//	printf("(i,low,upp) %6d %6d %6d\n", i, low, upp);
				assert(low + 1 < upp);

				// sort by CompK(2k)
				std::sort(sa.begin() + low, sa.begin() + upp, ck);

				// split equal(k) range to equal(2k) range
				int j = low;
				do {
					int sublow = j;
					int curr = sa[j]; // begin of equal(2k) range
					do {
						// `rank2[sa[j]]` is rank of suffix(j)
						rank2[sa[j]] = sublow;
						++j;
					} while (j != upp && ck.equal(curr, sa[j]));

					if (j - sublow > 1) {
						eq_range2.push_back(sublow);
						eq_range2.push_back(j);
					}
				} while (j != upp);
			}
			rank = rank2; // must be assign, not swap

			// `swap` is faster than `operator=`
			eq_range.swap(eq_range2);
		}
	}

	void verify()
	{
		for (ptrdiff_t i = 0, n = sa.size(); i != n; ++i)
		{
			if (rank[sa[i]] != i) {
				printf("%8d %8d %8d\n", (int)i, sa[i], rank[i]);
			}
		}
	}
};

//
//-----------------------------------------------------------------------------
//
suffix_array::suffix_array()
	: impl(new suffix_array_impl)
{
}
suffix_array::~suffix_array()
{
	delete impl;
}

void suffix_array::append(int ch)
{
	impl->append(ch);
}

const std::vector<int>& suffix_array::build()
{
	impl->build();
	return impl->sa;
}

const std::vector<int>& suffix_array::build_n(int FirstN)
{
	impl->build_n(FirstN);
	return impl->sa;
}

const std::vector<int>& suffix_array::build_o(int FirstN)
{
	impl->build_o(FirstN);
	return impl->sa;
}

//
//------------------------------------------------------------------------
//
/*
void suffix_tree::build(node* p)
{
//	root.resize(nlen);

	for (int i = 0, j; i != nlen; ++i)
	{
		pair<iter_t,bool> ib;
		j = 0;
		p = &root;
		do {
			ib = p->insert(base[i + j]);
			p = &ib.first->sub;
			++j;
		} while (!ib.second);

		ib.first->pos = i + j;
	}
}
*/
} // namespace terark




