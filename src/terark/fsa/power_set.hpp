#pragma once

#include <terark/gold_hash_map.hpp>
#include <terark/smallmap.hpp>
#include <boost/noncopyable.hpp>
#include <algorithm>
#include <utility>

namespace terark {

struct powerkey_t {
	size_t offset;
	size_t length;

	powerkey_t(size_t offset1, size_t length1)
	   	: offset(offset1), length(length1) {}
	powerkey_t() : offset(), length() {}
};

template<class PowerIndex>
struct PowerSetKeyExtractor {
	powerkey_t operator()(const PowerIndex& x0) const {
		// gold_hash is ValueInline, and Data==Link==PowerIndex
		// so next Index is at (&x0)[2], (&x0)[1] is hash link
		PowerIndex x1 = (&x0)[2];
	//	static long n = 0;
	//	printf("n=%ld x0=%ld x1=%ld\n", n++, (long)x0, (long)x1);
		return powerkey_t(x0, x1-x0);
	}
};

template<class Uint>
size_t uint_array_hash(const Uint* a, size_t n) {
	size_t h = 0;
	for (size_t i = 0; i < n; ++i) {
		h += a[i];
		h *= 31;
		h += h << 3 | h >> (sizeof(h)*8-3);
	}
	return h;
}
template<class Uint>
bool uint_array_equal(const Uint* xp, size_t xn, const Uint* yp, size_t yn) {
	if (xn != yn) return false;
	for (size_t i = 0; i < xn; ++i)
		if (xp[i] != yp[i]) return false;
	return true;
}

template<class U1, class U2>
inline size_t uint_pair_hash(const std::pair<U1,U2>& x) {
	size_t h = x.first;
	h *= 31;
	h += h << 3 | h >> (sizeof(h)*8-3);
	h += x.second;
	h *= 31;
	h += h << 3 | h >> (sizeof(h)*8-3);
	return h;
}

#if TERARK_WORD_BITS == 64
BOOST_STATIC_ASSERT(sizeof(size_t) == 8);
inline size_t uint_pair_hash(const std::pair<uint32_t, uint32_t>& x) {
	return (size_t(x.first) << 32) | x.second;
}
#endif

template<class U1, class U2>
inline bool
uint_pair_equal(const std::pair<U1,U2>& x, const std::pair<U1,U2>& y) {
	return x.first == y.first && x.second == y.second;
}

template<class U1, class U2>
struct uint_pair_hash_equal {
	inline size_t
	hash(const std::pair<U1,U2>& x) const { return uint_pair_hash(x); }
	inline bool
	equal(const std::pair<U1,U2>& x, const std::pair<U1,U2>& y) const {
	   	return uint_pair_equal(x, y); }
};

template<class StateID>
class SubSetHashMap : boost::noncopyable {
	static const StateID nil_link = StateID(-1);
	struct SubNode {
		StateID  index;
		StateID  hlink; // hash link
		SubNode(StateID i, StateID l) : index(i), hlink(l) {}
	};
	StateID* bucket;
	size_t  nBucket;
public:
	valvec<SubNode>  node;
	valvec<StateID>  data;

	size_t size() const { return node.size()-1; }

	size_t find_or_add_subset_sort_uniq(valvec<StateID>& ss) {
		std::sort(ss.begin(), ss.end());
		ss.trim(std::unique(ss.begin(), ss.end()));
		return find_or_add_subset(ss.data(), ss.size());
	}
	size_t find_or_add_subset(const valvec<StateID>& ss) {
		return find_or_add_subset(ss.data(), ss.size());
	}
	size_t find_or_add_subset(const StateID* ss, size_t sn) {
		assert(sn > 0);
		size_t hash = uint_array_hash(ss, sn);
		size_t hMod = hash % nBucket;
		size_t hit = bucket[hMod];
		while (nil_link != hit) {
			assert(hit < node.size()-1);
			size_t y0 = node[hit+0].index;
			size_t y1 = node[hit+1].index;
			size_t yn = y1 - y0;
			if (uint_array_equal(ss, sn, &data[y0], yn))
				return hit;
			hit = node[hit].hlink;
		}
		hit = node.size()-1;
		data.append(ss, sn);
		node.emplace_back((StateID)data.size(), nil_link);
		if (node.size() < nBucket*3/4) {
			node[hit].hlink = bucket[hMod];
			bucket[hMod] = (StateID)hit;
		} else { // rehash
			resize_bucket(nBucket+1);
		}
		return hit;
	}

	void resize_bucket(size_t n0) {
		if (bucket) free(bucket);
		nBucket = __hsm_stl_next_prime(n0);
		bucket = (StateID*)malloc(sizeof(StateID)*nBucket);
		if (NULL == bucket) THROW_STD(runtime_error, "bad_alloc");
		std::fill_n(bucket, nBucket, (StateID)nil_link);
		for(size_t jj = 0; jj < node.size()-1; ++jj) {
			size_t y0 = node[jj+0].index;
			size_t y1 = node[jj+1].index;
			size_t hash = uint_array_hash(&data[y0], y1-y0);
			size_t hMod = hash % nBucket;
			node[jj].hlink = bucket[hMod];
			bucket[hMod] = (StateID)jj;
		}
	}
	explicit SubSetHashMap(size_t nstates) : bucket(NULL), nBucket(0) {
		node.reserve(nstates*2);
		node.emplace_back(0, nil_link);
		resize_bucket(nstates*3/2);
	}
	~SubSetHashMap() { if (bucket) ::free(bucket); }
};
template<class StateID>
const StateID SubSetHashMap<StateID>::nil_link;

template<class StateID>
struct PowerSetHashEq {
	bool equal(powerkey_t x, powerkey_t y) const {
		assert(x.length > 0);
		assert(x.offset + x.length <= power_set.size());
		assert(y.length > 0);
		assert(y.offset + y.length <= power_set.size());
		if (x.length != y.length) {
			return false;
		}
		const StateID* p = power_set.data();
		size_t x0 = x.offset;
		size_t y0 = y.offset;
		size_t nn = x.length;
		do {
			if (p[x0] != p[y0])
				return false;
			++x0, ++y0;
		} while (--nn);
		return true;
	}
	size_t hash(powerkey_t x) const {
		assert(x.length > 0);
		assert(x.offset + x.length <= power_set.size());
		const StateID* p = power_set.data() + x.offset;
		size_t h = 0;
		size_t n = x.length;
		do {
			h += *p++;
			h *= 31;
			h += h << 3 | h >> (sizeof(h)*8-3);
		} while (--n);
		return h;
	}
	explicit PowerSetHashEq(const valvec<StateID>& power_set)
	 : power_set(power_set) {}

	const valvec<StateID>& power_set;
};

template<class StateID, class PowerIndex>
struct PowerSetMap {
	// must be FastCopy/realloc
	typedef node_layout<PowerIndex, PowerIndex, FastCopy, ValueInline> Layout;
	typedef PowerSetHashEq<StateID> HashEq;
	typedef PowerSetKeyExtractor<PowerIndex> KeyExtractor;
	typedef gold_hash_tab<powerkey_t, PowerIndex
				, HashEq, KeyExtractor, Layout
				>
			base_type;
	struct type : base_type {
		type(valvec<StateID>& power_set)
		  : base_type(64, HashEq(power_set), KeyExtractor())
		{}
	};
};

template<class StateID>
struct NFA_SubSet : valvec<StateID> {
	void resize0() {
		this->ch = -1;
		valvec<StateID>::resize0();
	}
	NFA_SubSet() { ch = -1; }
	ptrdiff_t  ch; // use ptrdiff_t for better align
};

} // namespace terark
