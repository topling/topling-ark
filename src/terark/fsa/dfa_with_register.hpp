#pragma once

#include <assert.h>
#include <stdio.h>

#include <typeinfo>
#include <boost/type_traits/is_integral.hpp>
#include <boost/static_assert.hpp>

namespace terark {

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
#pragma pack(push,1)
class State38 {
	typedef size_t StateID;
	enum { PtrBits =  38 };
	enum { nbmBits =   4 };
	enum { Sigma   = 256 };
public:
	size_t head : PtrBits;
	size_t next : PtrBits;
	#include "ppi/packed_state.hpp"
	State38() {
		head = nil_state;
		next = hash_delmark;
		init();
	}
	static const size_t max_state = nil_state - 1;
	static const size_t hash_delmark = max_state;
};
#pragma pack(pop)
BOOST_STATIC_ASSERT(sizeof(State38) == 16);

// now BaseDFA could only be Automata<...>
template<class BaseDFA>
class DFA_WithOnflyRegister : public BaseDFA {
protected:
    typedef typename BaseDFA::MyMemPool MyMemPool;
	using BaseDFA::pool;
	using BaseDFA::firstFreeState;
	using BaseDFA::numFreeStates;
public:
	typedef typename BaseDFA::state_t    state_t;
	typedef typename BaseDFA::state_id_t state_id_t;
	typedef typename BaseDFA::header_max header_max;
	static const bool   has_register = 1;
	static const size_t hash_delmark = state_t::hash_delmark;
	using BaseDFA::onfly_hash_hash;
	using BaseDFA::onfly_hash_equal;
	using BaseDFA::nil_state;
	using BaseDFA::sigma;
	using BaseDFA::states; // bring to public

	DFA_WithOnflyRegister() {
		m_num_registered = 0;
		states.risk_set_capacity(states.size());
	}
	size_t insert_at(size_t idx) {
		assert(idx < states.size());
		assert(!states[idx].is_free());
		assert(m_num_registered < states.size());
		assert(hash_delmark == states[idx].next);
		state_t* node = states.data();
		size_t hash = onfly_hash_hash(idx);
		size_t hMod = hash % states.capacity();
	//	printf("insert(idx=%zd) hash=%zd\n", idx, hash);
		size_t p = node[hMod].head;
		while (nil_state != p) {
			HSM_SANITY(p < states.size());
			if (onfly_hash_equal(idx, p))
				return p;
			p = node[p].next;
		}
		node[idx].next = node[hMod].head;
		node[hMod].head = idx; // new head of hMod'th bucket
		m_num_registered++;
		return idx;
	}

	///@returns number of erased elements
	size_t erase_i(size_t idx) {
	#if defined(_DEBUG) || !defined(NDEBUG)
		assert(idx < states.size());
		assert(!states[idx].is_free());
	#else
		if (idx >= std::min(states.size(), hash_delmark)) {
			throw std::invalid_argument(BOOST_CURRENT_FUNCTION);
		}
	#endif
		if (hash_delmark == states[idx].next)
			return 0;
		state_t* node = states.data();
		size_t hash = onfly_hash_hash(idx);
		size_t hMod = hash % states.capacity();
	//	printf("erase(idx=%zd) hash=%zd\n", idx, hash);
		HSM_SANITY(nil_state != node[hMod].head);
        size_t curr = node[hMod].head;
		if (curr == idx) {
			node[hMod].head = node[curr].next;
		} else {
			size_t prev;
			do {
				prev = curr;
				curr = node[curr].next;
				HSM_SANITY(curr < states.size());
			} while (idx != curr);
			node[prev].next = node[curr].next;
		}
		m_num_registered--;
		node[idx].next = hash_delmark;
		return 1;
	}

	void erase_all() {
		state_t* node = states.data();
		for (size_t i = 0, n = states.capacity(); i < n; ++i) {
			node[i].head = nil_state;
			node[i].next = hash_delmark;
		}
		m_num_registered = 0;
	}

	size_t total_states() const { return states.size(); }

	size_t new_state() override {
        size_t s;
        if (nil_state == firstFreeState) {
            assert(numFreeStates == 0);
			s = states.size();
			if (states.capacity() == s)
				resize_states_impl(s + 1);
			else
				states.risk_set_size(s + 1);
        } else {
            assert(numFreeStates > 0);
            numFreeStates--;
			assert(firstFreeState < states.size());
            s = firstFreeState;
            firstFreeState = states[s].get_target();
            states[s].init(); // don't touch 'head' and 'next'
        }
        return s;
	}

	void resize_states(size_t newsize) {
#if 1
		resize_states_impl(newsize);
#else
		size_t oldsize = states.size();
		resize_states_impl(newsize);
		if (oldsize == newsize)
			return;
		fprintf(stderr, "%s: old=%zd:%zd newsize=%zd\n"
			, BOOST_CURRENT_FUNCTION, states.size(), states.capacity()
			, newsize);
		state_t* node = states.data();
		for(size_t i = oldsize; i < newsize; ++i) node[i].set_target(i+1);
		if (nil_state == firstFreeState) {
			assert(0 == numFreeStates);
			firstFreeState = oldsize;
		} else {
			size_t prev, curr = firstFreeState;
			do {
				prev = curr;
				curr = node[curr].get_target();
			} while (nil_state != curr);
			node[prev].set_target(oldsize);
		}
		node[newsize-1].set_target(nil_state);
#endif
	}
	void resize_states_impl(size_t newsize) {
	// also do rehash
		size_t oldsize = states.size(), oldcap = states.capacity();
		if (newsize < oldsize) {
			THROW_STD(invalid_argument,
				   	"can't shrink, because there are free states");
		}
		if (newsize >= oldsize && newsize <= oldcap) {
			states.risk_set_size(newsize);
			return;
		}
		size_t newcap = __hsm_stl_next_prime(newsize);
		assert(states.capacity() < newcap);
		if (newcap >= hash_delmark)
			THROW_STD(out_of_range, "newsize=%zd newcap=%zd hash_delmark=%zd"
						, newsize, newcap, hash_delmark);
	//	fprintf(stderr, "%s: old=%zd:%zd new=%zd:%zd\n"
	//		, BOOST_CURRENT_FUNCTION, oldsize, oldcap, newsize, newcap);
		state_t* q = (state_t*)realloc(states.data(), sizeof(state_t)*newcap);
		if (NULL == q)
			THROW_STD(runtime_error, "realloc(%zd)", sizeof(state_t)*newcap);
		states.risk_set_data(q);
		states.risk_set_size(newsize);
		states.risk_set_capacity(newcap);
		for(size_t i = oldcap; i < newcap; ++i) {
			q[i].head = nil_state;
			q[i].next = hash_delmark;
			q[i].init();
		}
		for(size_t i = 0; i < oldcap; ++i) q[i].head = nil_state;
		for(size_t i = 0; i < oldsize; ++i) {
			if (hash_delmark == q[i].next)
				continue;
			size_t hash = onfly_hash_hash(i);
			size_t hMod = hash % newcap;
			q[i].next = q[hMod].head;
			q[hMod].head = i;
		}
	}

	// this method is non-const, different from BaseDFA
	// (*this) will be destroyed
	template<class ThatDFA>
	void give_to(ThatDFA* that) {
		typedef typename BaseDFA::transition_t SrcMove;
		typedef typename ThatDFA::transition_t DstMove;
		typedef typename BaseDFA::state_t      SrcState;
		typedef typename ThatDFA::state_t      DstState;
		BOOST_STATIC_ASSERT(boost::is_integral<SrcMove>::value);
		size_t size = states.size();
		while (size > 0 && states[size-1].is_free()) --size;
		assert(size < ThatDFA::max_state);
		if (size >= ThatDFA::max_state) {
			THROW_STD(out_of_range, "states=%zd ThatDFA::max_state=%zd"
					, size, size_t(ThatDFA::max_state)
					);
		}
		states.resize_no_init(size);
		states.shrink_to_fit();
		MyMemPool temp(sizeof(DstMove)*sigma + sigma/8);
		SrcState* large = this->states.data();
		DstState* small = reinterpret_cast<DstState*>(large);
		firstFreeState = ThatDFA::nil_state;
		numFreeStates = 0;
		for(size_t i = 0; i < size; ++i) {
			if (large[i].is_free()) {
				small[i].init();
				small[i].set_free();
				small[i].set_target(firstFreeState);
				firstFreeState = i;
				numFreeStates++;
			   	continue;
			}
			size_t   spos1 = large[i].spos;
			byte_t   nbm   = large[i].nbm;
			byte_t   pzip  = large[i].pzip_bit;
			byte_t   term  = large[i].term_bit;
			auchar_t minch = large[i].minch;
			if (spos1 != nil_state && spos1 >= ThatDFA::nil_state) {
				THROW_STD(out_of_range, "states[%zd(%zX)].spos=%zd"
						, i, i, spos1);
			}
#define TRIM_HASH_LINK_SET_STATE() \
			small[i].nbm      = nbm;  \
			small[i].pzip_bit = pzip; \
			small[i].term_bit = term; \
			small[i].minch    = minch

			if (!large[i].more_than_one_child()) {
				if (nil_state == spos1) {
					small[i].spos = ThatDFA::nil_state;
				} else {
					assert(spos1 < size);
					small[i].spos = spos1;
				}
				TRIM_HASH_LINK_SET_STATE();
				continue;
			}
			const int bits = large[i].rlen();
			size_t pos1 = large[i].getpos();
			size_t pos2, bm_size, nch;
			if (this->is_32bitmap(bits)) {
				nch = fast_popcount(pool.template at<uint32_t>(pos1));
				bm_size = 4;
			} else {
				const header_max& hb = pool.template at<header_max>(pos1);
				const int cBlock = hb.align_bits(bits) / hb.BlockBits;
				bm_size = cBlock * hb.BlockBits/8;
				nch = 0;
				for (int j = 0; j < cBlock; ++j)
					nch += fast_popcount(hb.block(j));
			}
			pos2 = temp.alloc(bm_size +  sizeof(DstMove) * nch);
			memcpy(temp.data() + pos2, pool.data() + pos1, bm_size);
			const
			SrcMove* src = &pool.template at<SrcMove>(pos1 + bm_size);
			DstMove* dst = &temp.template at<DstMove>(pos2 + bm_size);
			for(size_t j = 0; j < nch; ++j)
				dst[j] = src[j]; // SrcMove must be unsigned integer
			small[i].setpos(pos2);
			TRIM_HASH_LINK_SET_STATE();
		}
		pool.swap(temp);
		reinterpret_cast<BaseDFA*>(that)->risk_swap(*this);
		that->shrink_to_fit();
	}

private:
	size_t m_num_registered;
};

} // namespace terark
