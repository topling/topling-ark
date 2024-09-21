#pragma once

#include <assert.h>
#include <stdio.h>

#include <algorithm>
#include <functional>
#include <vector>
#include <string>
#include <typeinfo>

#include <terark/bitmap.hpp>
#include <terark/num_to_str.hpp>
#include <terark/hash_strmap.hpp>

#include "dfa_algo.hpp"
#include "dfa_mmap_header.hpp"
#include "dense_state_flags.hpp"
#include "hopcroft.hpp"
#include "nfa.hpp"
#include <terark/mempool_lock_none.hpp>

namespace terark {

#if 1 //defined(NDEBUG)
  #define DenseDFA_Check(s)
  #define DenseDFA_CheckAll()
#else
  #define DenseDFA_Check(s) \
	do { \
		size_t n_sparse1 = s.num_sparse(); \
		assert(n_sparse1 <= sigma); \
		if (n_sparse1) { \
			assert(nil_state != s.m_pos); \
			const StateID* trans = &pool.template at<StateID>(s.getpos()); \
			for (size_t i = 0; i < n_sparse1; ++i) { \
				assert(trans[i] < states.size()); \
				assert(!states[trans[i]].is_free()); \
			} \
		} else assert(nil_state == s.m_pos); \
		for(size_t i = 0; i < MyBitMap::BlockN; ++i) { \
			bm_uint_t dense = s.dense.block(i); \
			bm_uint_t sparse = s.sparse.block(i); \
			assert((dense|sparse) == (dense^sparse)); \
		} \
		assert((s.dense.is_all0() && nil_state == s.dense_target1) \
			 ||(s.dense.has_any1() && nil_state != s.dense_target1) \
			 ); \
		assert(n_sparse1 + s.dense.popcnt() <= sigma); \
		assert((nil_state == s.m_pos && 0 == n_sparse1) \
			 ||(nil_state != s.m_pos && 0 != n_sparse1) \
			 ); \
	} while (0)

  #define DenseDFA_CheckAll() \
	do { \
		for (size_t k = 0; k < states.size(); ++k) { \
			const State& s2 = states[k]; \
			DenseDFA_Check(s2); \
		} \
	} while (0)
#endif

#pragma pack(push,1)
template<class StateID, int Sigma
		,class StateFlags = DenseStateFlags<StateID>
		>
struct DenseState : public StateFlags {
	typedef static_bitmap<Sigma, bm_uint_t> MyBitMap;
	enum { PoolAlign = sizeof(StateID) };
	static const StateID nil_state = StateID(-1);
	static const StateID max_state = StateID(-1) - 1;
	StateID  dense_target1;
	StateID  dense_target2;
	StateID  m_pos;
	MyBitMap dense;
	MyBitMap sparse;

	void setpos(size_t x) {
		assert(x / PoolAlign < max_state);
		m_pos = x / PoolAlign;
	}
	size_t getpos() const { return m_pos * PoolAlign; }

	int num_children() const {
		int n = 0;
		for (int i = 0; i < MyBitMap::BlockN; ++i) {
			bm_uint_t bm = dense.block(i) | sparse.block(i);
			n += fast_popcount(bm);
		}
		assert(n <= Sigma);
		return n;
	}

	int num_sparse() const {
		int n = 0;
		for (int i = 0; i < MyBitMap::BlockN; ++i) {
			bm_uint_t d =  dense.block(i);
			bm_uint_t s = sparse.block(i);
			bm_uint_t x = (d ^ s) & s;
			n += fast_popcount(x);
		}
		return n;
	}

	int idx_sparse(int c) const {
		assert(c >= 0);
		assert(c < Sigma);
		int idx = 0;
		int n = c / MyBitMap::BlockBits;
		for (int i = 0; i < MyBitMap::BlockN; ++i) {
			if (i < n) {
				bm_uint_t d =  dense.block(i);
				bm_uint_t s = sparse.block(i);
				bm_uint_t x = (d ^ s) & s;
				idx += fast_popcount(x);
			} else break;
		}
		if (int shift = c % MyBitMap::BlockBits) {
			bm_uint_t d =  dense.block(n);
			bm_uint_t s = sparse.block(n);
			bm_uint_t x = (d ^ s) & s;
			idx += fast_popcount_trail(x, shift);
		}
		return idx;
	}

	int num_dense1() const {
		int n = 0;
		for (int i = 0; i < MyBitMap::BlockN; ++i) {
			bm_uint_t d =  dense.block(i);
			bm_uint_t s = sparse.block(i);
			bm_uint_t x = (d ^ s) & d;
			n += fast_popcount(x);
		}
		return n;
	}

	int num_dense2() const {
		int n = 0;
		for (int i = 0; i < MyBitMap::BlockN; ++i) {
			bm_uint_t d =  dense.block(i);
			bm_uint_t s = sparse.block(i);
			n += fast_popcount(s & d);
		}
		return n;
	}

	auchar_t get_single_child_char() const {
		assert(dense.popcnt() == 1);
		assert(sparse.popcnt() == 0);
		assert(dense_target2 == nil_state);
		for(size_t i = 0; i < MyBitMap::BlockN; ++i) {
			bm_uint_t bm = dense.block(i);
			if (bm) {
				auchar_t c = i * MyBitMap::BlockBits + fast_ctz(bm);
				return c;
			}
		}
		assert(0); // never goes here
		abort();
		return 0;
	}

	DenseState() : dense(0), sparse(0) {
		dense_target1 = nil_state;
		dense_target2 = nil_state;
		m_pos = nil_state;
	}
};
#pragma pack(pop)

template< class StateID, int Sigma = 256
	    , class State = DenseState<StateID, Sigma>
	   	>
class DenseDFA : public BaseDFA, public DFA_MutationInterface
{
public:
	static const bool has_register = 0;
	enum { PoolAlign = sizeof(StateID) };
	enum { char_bits = 8 };
	enum { sigma = Sigma };
    typedef MemPool_LockNone<PoolAlign> MyMemPool;
	typedef StateID state_id_t;
	typedef StateID transition_t;
	typedef typename State::MyBitMap MyBitMap;
	static const StateID nil_state = State::nil_state;
	static const StateID max_state = State::max_state;
	typedef State state_t;

	bool has_freelist() const override final { return true; }

	void init() {
		m_atom_dfa_num = 0;
		m_dfa_cluster_num = 0;
		m_dyn_sigma = sigma;
        states.push_back(State()); // initial_state
        firstFreeState = nil_state;
        numFreeStates  = 0;
        transition_num = 0;
		m_zpath_states = 0;
		m_total_zpath_len = 0;
	}

public:
    DenseDFA() : pool(sizeof(StateID)*sigma) { init(); }
	void clear() { states.clear(); pool.clear(); init(); }
	void erase_all() { states.erase_all(); pool.erase_all(); init(); }

	const valvec<State>& internal_get_states() const { return states; }
	const MyMemPool    & internal_get_pool  () const { return pool  ; }
    size_t total_states() const { return states.size(); }
    size_t total_transitions() const { return transition_num; }
    size_t new_state() override final {
        state_id_t s;
        if (nil_state == firstFreeState) {
            assert(numFreeStates == 0);
            s = states.size();
            if (s >= max_state) {
                throw std::runtime_error("DenseDFA::new_state(), exceed State::max_state");
            }
            states.push_back(State());
        } else {
            assert(numFreeStates > 0);
            numFreeStates--;
            s = firstFreeState;
            firstFreeState = states[s].dense_target1;
            new(&states[s])State(); // call default constructor
        }
        return s;
    }
    size_t clone_state(size_t source) override final {
        assert(source < states.size());
        size_t y = new_state();
        const State& s = states[source];
		states[y] = s;
		size_t n_sparse = s.num_sparse();
		if (n_sparse) {
			assert(nil_state != s.m_pos);
			size_t pos = pool.alloc(sizeof(StateID) * n_sparse);
			states[y].setpos(pos);
			StateID* p = &pool.template at<StateID>(s.getpos());
			StateID* q = &pool.template at<StateID>(pos);
			memcpy(q, p, sizeof(StateID) * n_sparse);
		}
		transition_num += n_sparse + s.dense.popcnt();
		return y;
	}

    void del_state(size_t s) override final {
		assert(0);
		assert(0 == m_zpath_states);
        assert(s < states.size());
        assert(s != initial_state); // initial_state shouldn't be deleted
		assert(!states[s].is_pzip()); // must be non-path-zipped
		del_all_move(s);
		states[s].dense_target1 = firstFreeState;
		states[s].set_free_bit();
		firstFreeState = s;
		numFreeStates++;
	}

	void del_all_move(size_t s) override final {
		assert(0 == m_zpath_states);
        assert(s < states.size());
		State& x = states[s];
		assert(!x.is_pzip()); // must be non-path-zipped
		if (nil_state != x.dense_target1) {
			transition_num -= x.dense.popcnt();
		}
		if (nil_state != x.m_pos) {
			size_t n_sparse = x.num_sparse();
			assert(n_sparse > 0);
			pool.sfree(x.getpos(), sizeof(StateID) * n_sparse);
			transition_num -= n_sparse;
		}
	//	new(&x) State ();
		x.dense.fill(0);
		x.sparse.fill(0);
		x.dense_target1 = nil_state;
		x.dense_target2 = nil_state;
		x.m_pos = nil_state;
	}

	state_id_t min_free_state() const {
		state_id_t s = firstFreeState;
		state_id_t s_min = states.size()-1;
		while (nil_state != s) {
			s_min = std::min(s, s_min);
			s = states[s].dense_target1;
		}
		return s_min;
	}
	size_t num_free_states() const { return numFreeStates; }
	size_t num_used_states() const { return states.size() - numFreeStates; }
	size_t v_gnode_states() const final { return num_used_states(); }

	bool is_free(StateID s) const {
	   	assert(s < states.size());
	   	return states[s].is_free();
   	}
	bool is_pzip(state_id_t s) const {
		return states[s].is_pzip();
	}
	bool is_term(StateID s) const {
	   	assert(s < states.size());
	   	return states[s].is_term();
   	}
	void set_term_bit(StateID s) {
	   	assert(s < states.size());
	   	states[s].set_term_bit();
   	}
	void clear_term_bit(StateID s) {
	   	assert(s < states.size());
	   	states[s].clear_term_bit();
   	}

	bool v_has_children(size_t s) const override final {
		assert(s < states.size());
		const State& x = states[s];
		return x.dense.has_any1() || x.sparse.has_any1();
	}
	bool has_children(state_id_t s) const {
		assert(s < (state_id_t)states.size());
		const State& x = states[s];
		return x.dense.has_any1() || x.sparse.has_any1();
	}
	bool more_than_one_child(state_id_t s) const {
        assert(s < (state_id_t)states.size());
		const State& x = states[s];
		return x.dense.popcnt() + x.sparse.popcnt() > 1;
	}

	void set_single_child(state_id_t parent, state_id_t child) {
		assert(0 == m_zpath_states);
        assert(parent < (state_id_t)states.size());
        assert(child  < (state_id_t)states.size());
		assert(states[parent].dense.popcnt() == 1);
		assert(states[parent].sparse.popcnt() == 0);
		assert(states[parent].dense_target2 == nil_state);
		states[parent].dense_target1 = child;
	}
	state_id_t get_single_child(state_id_t parent) const {
		assert(0 == m_zpath_states);
        assert(parent < (state_id_t)states.size());
		assert(states[parent].dense.popcnt() == 1);
		assert(states[parent].sparse.popcnt() == 0);
		assert(states[parent].dense_target2 == nil_state);
		return states[parent].dense_target1;
	}
	auchar_t get_single_child_char(state_id_t parent) const {
		assert(0 == m_zpath_states);
        assert(parent < (state_id_t)states.size());
	    return states[parent].get_single_child_char();
	}

    state_id_t state_move(state_id_t curr, auchar_t ch) const {
        assert(curr < (state_id_t)states.size());
		ASSERT_isNotFree(curr);
        return state_move(states[curr], ch);
	}
    state_id_t state_move(const State& s, auchar_t ch) const {
		if (s.dense.is1(ch)) {
			if (s.sparse.is1(ch)) {
			//	assert(s.dense_target2 < states.size());
				return s.dense_target2;
			} else {
			//	assert(s.dense_target1 < states.size());
				return s.dense_target1;
			}
		}
		if (s.sparse.is1(ch)) {
			size_t idx = s.idx_sparse(ch);
			assert(idx < sigma);
			const StateID* trans = &pool.template at<StateID>(s.getpos());
		#if !defined(NDEBUG)
			size_t n_sparse = s.num_sparse();
			for (size_t i = 0; i < n_sparse; ++i) {
			//	assert(trans[i] < states.size());
			}
		#endif
			return trans[idx];
		}
		return nil_state;
	}

	void del_move(size_t s, auchar_t ch) override final {
		assert(ch < Sigma);
        assert(s < (state_id_t)states.size());
		State& x = states[s];
		assert(!x.is_free());
		assert(x.dense.is1(ch) || x.sparse.is1(ch));
		if (x.dense.is1(ch)) {
			x.dense.set0(ch);
			if (x.dense.is_all0()) {
				x.dense_target1 = nil_state;
				x.dense_target2 = nil_state;
			}
			x.sparse.set0(ch);
			transition_num--;
			return;
		}
		assert(x.sparse.is1(ch));
		if (x.sparse.is0(ch)) {
			return;
		}
		size_t n_sparse = x.num_sparse();
		size_t oldsize = sizeof(StateID) * n_sparse;
		size_t oldpos = x.getpos();
		assert(n_sparse > 0);
		if (1 == n_sparse) {
			pool.sfree(oldpos, oldsize);
			x.m_pos = nil_state;
		}
		else {
			StateID* trans = &pool.template at<StateID>(oldpos);
			for (size_t i = x.idx_sparse(ch)+1; i < n_sparse; ++i)
				trans[i-1] = trans[i]; // copy forward, delete ch
			size_t newsize = sizeof(StateID) * (n_sparse-1);
			size_t newpos = pool.alloc3(oldpos, oldsize, newsize);
			x.setpos(newpos);
		}
		x.sparse.set0(ch);
		transition_num--;
	}

	using DFA_MutationInterface::add_all_move;
	void add_all_move(size_t s, const CharTarget<size_t>* trans, size_t n)
	override final {
		State& x = states[s];
		assert(x.dense.is_all0());
		assert(x.sparse.is_all0());
		assert(!x.is_free());
		assert(!x.is_pzip());
		assert(nil_state == x.m_pos);
		assert(nil_state == x.dense_target1);
		assert(nil_state == x.dense_target2);
		assert(n <= sigma);
		if (0 == n) return; // do nothing
		size_t max_freq1 = 0; StateID max_dest1 = nil_state;
		size_t max_freq2 = 0; StateID max_dest2 = nil_state;
		{
			StateID children[sigma];
			for(size_t i = 0; i < n; ++i)
				children[i] = trans[i].target;
			std::sort(children, children+n);
			for(size_t i = 0; i < n; ) {
				size_t j = i+1;
				while (j < n && children[j] == children[i]) ++j;
				if (max_freq1 < j - i) {
					if (max_freq2 < max_freq1) {
						max_freq2 = max_freq1;
						max_dest2 = max_dest1;
					}
					max_freq1 = j - i;
					max_dest1 = children[i];
				}
			   	else if (max_freq2 < j - i) {
					max_freq2 = j - i;
					max_dest2 = children[i];
				}
				i = j;
			}
			assert(max_freq1 + max_freq2 <= n);
		}
		StateID* p_sparse = NULL;
		size_t alsize = 0;
		size_t n_sparse = n - max_freq1 - max_freq2;
		if (n_sparse > 0) {
			alsize = pool.align_to(sizeof(StateID) * n_sparse);
			size_t pos = pool.alloc(alsize);
			p_sparse = &pool.template at<StateID>(pos);
			x.setpos(pos);
		}
		size_t j = 0;
		for(size_t i = 0; i < n; ++i) {
			const auto ct = trans[i];
			if (ct.target == max_dest1) {
				x.dense.set1(ct.ch);
			} else if (ct.target == max_dest2) {
				x.dense.set1(ct.ch);
				x.sparse.set1(ct.ch);
			} else {
				x.sparse.set1(ct.ch);
				p_sparse[j++] = ct.target;
			}
		}
		x.dense_target1 = max_dest1;
		x.dense_target2 = max_dest2;
		transition_num += n;
		assert(j == n_sparse);
		assert(x.num_sparse() == (int)n_sparse);
		assert(x.num_dense1() == (int)max_freq1);
		assert(x.num_dense2() == (int)max_freq2);
	}

protected:
    size_t
	add_move_imp(size_t source, size_t target, auchar_t ch, bool OverwriteExisted)
	override final {
		static size_t s_cnt = 0;
		s_cnt++;
        assert(source < states.size());
	//	assert(target < states.size());
		ASSERT_isNotFree(source);
	//	ASSERT_isNotFree(target);
		assert(!states[source].is_pzip());
        State& s = states[source];
		StateID old_target = nil_state;
		DenseDFA_CheckAll();
		size_t idx = s.idx_sparse(ch);
		size_t n_sparse = s.num_sparse();
		if (s.dense.is1(ch)) {
			assert(s.dense_target1 != nil_state);
			if (s.sparse.is0(ch) && target == s.dense_target1)
				return target;
			if (s.sparse.is1(ch) && target == s.dense_target2)
				return target;
			if (OverwriteExisted) {
				if (s.sparse.is1(ch)) {
					if (target == s.dense_target1) {
						s.sparse.set0(ch); // set target as dense_target1
						return s.dense_target2; // old is dense_target2
					}
					old_target = s.dense_target2;
				}
				else {
					if (target == s.dense_target2) {
						s.sparse.set1(ch); // set target as dense_target2
						return s.dense_target1; // old is dense_target1
					}
					old_target = s.dense_target1;
				}
			   	s.dense.set0(ch); // will insert into sparse targets
				s.sparse.set0(ch);
				transition_num--;
			} else
				return s.dense_target1;
		}
		else if (target == s.dense_target1 || nil_state == s.dense_target1
			  || target == s.dense_target2 || nil_state == s.dense_target2)
		{
			if (s.sparse.is1(ch)) {
				assert(n_sparse > 0);
				StateID* trans = &pool.template at<StateID>(s.getpos());
				old_target = trans[idx];
				assert(old_target != target);
				if (old_target == target) {
					abort();
					return nil_state;
				}
				if (OverwriteExisted) {
					// delete from sparse targets:
					size_t oldsize = sizeof(StateID) * (n_sparse+0);
					if (1 == n_sparse) {
						pool.sfree(s.getpos(), oldsize);
						s.m_pos = nil_state;
					} else {
						for (size_t i = idx+1; i < n_sparse; ++i)
							trans[i-1] = trans[i]; // copy forward, remove idx
						size_t newsize = sizeof(StateID) * (n_sparse-1);
						size_t newpos = pool.alloc3(s.getpos(), oldsize, newsize);
						s.setpos(newpos);
					}
					if (target == s.dense_target1) {
						s.sparse.set0(ch);
					}
					else if (nil_state == s.dense_target1) {
						s.sparse.set0(ch);
						s.dense_target1 = target;
					}
					else if (nil_state == s.dense_target2) {
						s.dense_target2 = target;
					}
					s.dense.set1(ch);
					DenseDFA_CheckAll();
				}
				else { // couldn't overwrite existed
					// no any change is needed
				}
			} else {
				transition_num++;
				s.dense.set1(ch);
				if (target == s.dense_target1) {}
				else if (nil_state == s.dense_target1)
					s.dense_target1 = target;
				else {
					s.sparse.set1(ch);
					s.dense_target2 = target;
				}
				DenseDFA_CheckAll();
			}
			return old_target; // maybe nil_state
		}
		else if (s.sparse.is1(ch)) {
			StateID* trans = &pool.template at<StateID>(s.getpos());
			old_target = trans[idx];
			if (OverwriteExisted)
				trans[idx] = target;
			DenseDFA_CheckAll();
			return old_target; // maybe nil_state
		}
		assert(s.sparse.is0(ch));
		DenseDFA_CheckAll();
		size_t pos;
		if (nil_state == s.m_pos) {
			assert(0 == n_sparse);
			assert(0 == idx);
			pos = pool.alloc(sizeof(StateID)*1);
			pool.template at<StateID>(pos) = target;
		}
		else {
			assert(0 != n_sparse);
			size_t oldpos = s.getpos();
			size_t oldsize = sizeof(StateID) * (n_sparse+0);
			size_t newsize = sizeof(StateID) * (n_sparse+1);
			pos = pool.alloc3(oldpos, oldsize, newsize);
			StateID* trans = &pool.template at<StateID>(pos);
			for (size_t j = n_sparse; j > idx; --j)
				trans[j] = trans[j-1]; // copy backward
			trans[idx] = target; // insert
		}
		s.sparse.set1(ch);
		s.setpos(pos);
		transition_num++;
		DenseDFA_CheckAll();
		return old_target; // maybe nil_state
	}

public:
	size_t v_num_children(size_t s) const override { return num_children(s); }
	size_t num_children(size_t curr) const {
		assert(curr < states.size());
		return states[curr].num_children();
	}

    template<class OP> // use ctz (count trailing zero)
	void for_each_dest_rev(state_id_t curr, OP op) const {
		ASSERT_isNotFree(curr);
		StateID dests[sigma];
		size_t  n = 0;
		for_each_dest(curr,
			[&](StateID t) {
				assert(n < sigma);
				dests[n++] = t;
			});
		for (size_t i = n; i > 0; --i) op(dests[i-1]);
	}
    template<class OP> // use ctz (count trailing zero)
	void for_each_dest(state_id_t curr, OP op) const {
		for_each_move(curr, [&](StateID t, auchar_t){op(t);});
	}
    template<class OP> // use ctz (count trailing zero)
	void for_each_move(state_id_t curr, OP op) const {
		ASSERT_isNotFree(curr);
        const State& s = states[curr];
		const StateID* trans = NULL;
	#ifndef NDEBUG
		DenseDFA_Check(s);
		size_t n_sparse = s.num_sparse();
	#endif
		if (nil_state != s.m_pos) {
			// when s.is_pzip()==true, 'trans' is a false trans
			// and trans will not be used in following loop
			// use (nil_state != s.m_pos) as the condition is
			// just for performance reason, computing n_sparse is slower
			assert(n_sparse > 0 || s.is_pzip());
			trans = &pool.template at<StateID>(s.getpos());
		} else {
			assert(0 == n_sparse && !s.is_pzip());
		}
		const StateID dense1 = s.dense_target1;
		const StateID dense2 = s.dense_target2;
		size_t pos = 0;
		for(size_t i = 0; i < MyBitMap::BlockN; ++i) {
			bm_uint_t const dense = s.dense.block(i);
			bm_uint_t const sparse = s.sparse.block(i);
			bm_uint_t const one = bm_uint_t(1);
			auchar_t  const mask = MyBitMap::BlockBits-1;
			bm_uint_t bm = dense | sparse;
			auchar_t  ch = i * MyBitMap::BlockBits;
			while (bm) {
				int ctz = fast_ctz(bm);
				ch += ctz;
				StateID target;
			   	if (dense & one<<(ch&mask)) {
					target = (sparse & one<<(ch&mask)) ? dense2 : dense1;
				//	assert(target < states.size());
			   	} else {
				   	assert(NULL != trans);
					assert(pos < n_sparse);
					assert(sparse & one<<(ch&mask));
				   	target = trans[pos++];
				//	assert(target < states.size());
			   	}
				op(target, ch);
				ch++;
				bm >>= ctz; // must not be bm >>= ctz + 1
				bm >>= 1;   // because ctz + 1 may be bits of bm(32 or 64)
			}
			assert(ch <= (i+1) * MyBitMap::BlockBits);
		}
		assert(pos == n_sparse);
 	}

    void resize_states(size_t new_states_num) override final {
		if (new_states_num >= max_state)
			throw std::logic_error("DenseDFA::resize_states: exceed max_state");
        states.resize(new_states_num);
    }

    size_t mem_size() const override {
        return pool.size() + sizeof(State) * states.size();
    }
	size_t frag_size() const { return pool.frag_size(); }

	void shrink_to_fit() {
		states.shrink_to_fit();
		pool.shrink_to_fit();
	}

	struct by_second_desc {
		template<class Pair>
		bool operator()(const Pair& x, const Pair& y) const {
			return x.second > y.second;
		}
	};

	void compact() {
		gold_hash_map<StateID, int> freq;
		MyMemPool tmp(sizeof(StateID) * sigma);
#define DenseDFA_compact_leftbrace { // for editor's auto brace match
#if 1
		for (size_t i = 0; i < states.size(); ++i)DenseDFA_compact_leftbrace
			if (states[i].is_free()) continue;
#else
		DFS_GraphWalker<StateID> walker;
		walker.resize(states.size());
		walker.putRoot(initial_state);
		while (!walker.is_finished())DenseDFA_compact_leftbrace
			size_t i = walker.next();
		   	walker.putChildren(this, (StateID)i);
#endif
			State& s = states[i];
			int n_sparse = s.num_sparse();
			assert(s.num_children() == s.dense.popcnt() + n_sparse);
			if (0 == n_sparse && !s.is_pzip()) {
				assert(nil_state == s.m_pos);
				continue;
			}
			assert(nil_state != s.m_pos);
			freq.erase_all();
			StateID* trans = &pool.template at<StateID>(s.getpos());
			if (nil_state != s.dense_target1) {
				int d = s.num_dense1(); assert(d);
			   	if (d) freq[s.dense_target1] = d;
			}
			if (nil_state != s.dense_target2) {
				int d = s.num_dense2(); assert(d);
			   	if (d) freq[s.dense_target2] = d;
			}
			for (int j = 0; j < n_sparse; ++j) {
				StateID t = trans[j];
			//	assert(t < states.size());
				assert(t != s.dense_target1 && t != s.dense_target2);
				freq[t]++;
			}
			int max_freq1 = 0; StateID max_dest1 = nil_state;
			int max_freq2 = 0; StateID max_dest2 = nil_state;
			for (size_t j = 0; j < freq.end_i(); ++j) {
				if (max_freq1 < freq.val(j)) {
					if (max_freq2 < max_freq1) {
						max_freq2 = max_freq1;
						max_dest2 = max_dest1;
					}
					max_freq1 = freq.val(j);
					max_dest1 = freq.key(j);
				}
				else if (max_freq2 < freq.val(j)) {
					max_freq2 = freq.val(j);
					max_dest2 = freq.key(j);
				}
			}
			int n_sparse2 = n_sparse + s.dense.popcnt() - max_freq1 - max_freq2;
			int k = 0;
			int zlen = 0;
			const byte_t* strp = NULL;
			size_t alsize = sizeof(StateID) * n_sparse2;
			if (s.is_pzip()) {
				fstring zs = get_zpath_data(i, NULL);
				strp = (const byte_t*)zs.data() - 1; // include len byte
				zlen = zs.size();
				alsize = pool.align_to(alsize + zlen + 1);
			} else {
				alsize = pool.align_to(alsize);
			}
			size_t newpos = tmp.alloc(alsize);
			trans = &tmp.template at<StateID>(newpos);
			MyBitMap dense(0), sparse(0);
			for_each_move(i, [&](StateID t, auchar_t c) {
					if (max_dest1 == t) {
						dense.set1(c);
					} else if (max_dest2 == t) {
						dense.set1(c);
						sparse.set1(c);
					} else {
						trans[k++] = t;
						sparse.set1(c);
					}
				});
			assert(k == n_sparse2);
			if (strp)
				memcpy(trans+n_sparse, strp, zlen+1);
			s.dense_target1 = max_dest1;
			s.dense_target2 = max_dest2;
			s.dense = dense;
			s.sparse = sparse;
			s.setpos(newpos);
			assert(s.num_children() == n_sparse2 + max_freq1 + max_freq2);
			assert(s.num_dense1() == max_freq1);
			assert(s.num_dense2() == max_freq2);
			assert(s.num_sparse() == n_sparse2);
		}
		tmp.shrink_to_fit();
		tmp.swap(pool);
	}

	void print_stat(FILE* fp) const {
		hash_strmap<int> bmcnt1, bmcnt2, bmcnt3;
		gold_hash_map<StateID, int> freq;
		for (size_t i = 0; i < states.size(); ++i) {
			freq.erase_all();
			State& s = states[i];
			if (nil_state != s.dense_target1)
				freq[s.dense_target1] = s.num_dense1();
			if (nil_state != s.dense_target2)
				freq[s.dense_target2] = s.num_dense2();
			int n_sparse = s.num_sparse();
			assert(s.num_children() == s.dense.popcnt() + n_sparse);
			if (n_sparse) {
				assert(nil_state != s.m_pos);
				StateID* trans = &pool.template at<StateID>(s.getpos());
				for (int j = 0; j < n_sparse; ++j) {
					StateID t = trans[j];
					assert(t < states.size());
					assert(t != s.dense_target1 && t != s.dense_target2);
					freq[t]++;
				}
			}
			bmcnt1[fstring((char*)&s.dense, 64)]++;
			bmcnt2[fstring((char*)&s.dense, 32)]++;
			bmcnt3[fstring((char*)&s.sparse, 32)]++;
			int d1 = s.num_dense1();
			int d2 = s.num_dense2();
			fprintf(fp, "%zd %d %d %d %zd\n", i, d1, d2, n_sparse, freq.size());
			// after sort, freq is no longer a valid gold_hash_map
			std::sort(freq.begin(), freq.end(), by_second_desc());
			for (size_t j = 0; j < freq.end_i(); ++j)
				fprintf(fp, "%zd %d %zd\n", i, freq.val(j), (size_t)freq.key(j));
		}
		for (size_t j = 0; j < bmcnt1.end_i(); ++j)
			fprintf(fp, "j=%zd bmcnt1=%d\n", j, bmcnt1.val(j));
		for (size_t j = 0; j < bmcnt2.end_i(); ++j)
			fprintf(fp, "j=%zd bmcnt2=%d\n", j, bmcnt2.val(j));
		for (size_t j = 0; j < bmcnt3.end_i(); ++j)
			fprintf(fp, "j=%zd bmcnt3=%d\n", j, bmcnt3.val(j));
	}

    void swap(DenseDFA& y) {
		assert(typeid(*this) == typeid(y));
		risk_swap(y);
	}
    void risk_swap(DenseDFA& y) {
		BaseDFA::risk_swap(y);
        pool  .swap(y.pool  );
        states.swap(y.states);
		std::swap(firstFreeState, y.firstFreeState);
		std::swap(numFreeStates , y.numFreeStates );
        std::swap(transition_num, y.transition_num);
    }

    void add_zpath(state_id_t s, const byte_t* str, size_t len) {
        assert(len <= 255);
		State& x = states[s];
		assert(!x.is_pzip());
		size_t newpos;
		size_t n_sparse = x.num_sparse();
		size_t oldlen = sizeof(StateID) * n_sparse; // unaligned
		size_t newlen = pool.align_to(oldlen + len + 1); // aligned
		if (0 == n_sparse) {
			assert(nil_state == x.m_pos);
			newpos = pool.alloc(newlen);
		} else {
			size_t aloldlen = pool.align_to(oldlen); // aligned
			newpos = pool.alloc3(x.getpos(), aloldlen, newlen);
		}
		byte_t* p = &pool.template at<byte_t>(newpos + oldlen);
		x.setpos(newpos);
		p[0] = len;
		memcpy(p+1, str, len);
		x.set_pzip_bit();
    }

	fstring get_zpath_data(size_t s, MatchContext*) const {
		assert(s < states.size());
		assert(!states[s].is_free());
		assert(states[s].is_pzip());
		size_t pos = states[s].getpos();
		size_t n_sparse = states[s].num_sparse();
		const char*  zbeg = reinterpret_cast<const char*>(pool.data());
		const size_t zpos = pos + sizeof(StateID) * n_sparse;
		return fstring(zbeg + zpos+1, pool.byte_at(zpos));
	}

private:
	enum { SERIALIZATION_VERSION = 2 };

	template<class DataIO> void load_au(DataIO& dio, size_t version) {
		typename DataIO::my_var_uint64_t n_pool, n_states;
		typename DataIO::my_var_uint64_t n_firstFreeState, n_numFreeStates;
		typename DataIO::my_var_uint64_t n_zpath_states;
		dio >> n_zpath_states;
		dio >> n_pool;
		dio >> n_states;
		dio >> n_firstFreeState;
		dio >> n_numFreeStates;
		if (version >= 2)
			load_kv_delim_and_is_dag(this, dio);
		m_zpath_states = n_zpath_states;
		states.resize_no_init(n_states.t);
		dio >> pool;
		dio.ensureRead(&states[0], sizeof(State)*n_states.t);
		if (version < 2) {
			this->m_is_dag = tpl_is_dag();
			// m_kv_delim is absent in version < 2
		}
	}
	template<class DataIO> void save_au(DataIO& dio) const {
		dio << typename DataIO::my_var_uint64_t(m_zpath_states);
		dio << typename DataIO::my_var_uint64_t(pool  .size());
		dio << typename DataIO::my_var_uint64_t(states.size());
		dio << typename DataIO::my_var_uint64_t(firstFreeState);
		dio << typename DataIO::my_var_uint64_t(numFreeStates);
		dio << uint16_t(this->m_kv_delim); // version >= 2
		dio << char(this->m_is_dag); // version >= 2
		dio << pool;
		dio.ensureWrite(&states[0], sizeof(State) * states.size());
	}

	template<class DataIO>
	friend void DataIO_loadObject(DataIO& dio, DenseDFA& au) {
		typename DataIO::my_var_uint64_t version;
		dio >> version;
		if (version > SERIALIZATION_VERSION) {
			typedef typename DataIO::my_BadVersionException bad_ver;
			throw bad_ver(version.t, SERIALIZATION_VERSION, "DenseDFA");
		}
		au.load_au(dio, version.t);
	}

	template<class DataIO>
	friend void DataIO_saveObject(DataIO& dio, const DenseDFA& au) {
		dio << typename DataIO::my_var_uint64_t(SERIALIZATION_VERSION);
		au.save_au(dio);
	}

public:
	~DenseDFA() {
		if (this->mmap_base) {
			pool.get_data_byte_vec().risk_release_ownership();
			states.risk_release_ownership();
		}
	}

	size_t m_atom_dfa_num;
	size_t m_dfa_cluster_num;

protected:
	MyMemPool pool;
    valvec<State> states;
    state_id_t    firstFreeState;
    state_id_t    numFreeStates;
    size_t transition_num;
	using BaseDFA::m_zpath_states;

	friend size_t
	GraphTotalVertexes(const DenseDFA* au) { return au->states.size(); }

public:
	size_t hopcroft_hash(size_t x_id) const;
	size_t onfly_hash_hash(size_t x_id) const;
	bool onfly_hash_equal(size_t x_id, size_t y_id) const;
	size_t adfa_hash_hash(const StateID* Min, size_t state_id) const;
	bool adfa_hash_equal(const StateID* Min, size_t x_id, size_t y_id) const;

typedef DenseDFA MyType;
//#include "ppi/for_each_suffix.hpp"
//#include "ppi/match_key.hpp"
//#include "ppi/match_path.hpp"
//#include "ppi/match_prefix.hpp"
#include "ppi/dfa_reverse.hpp"
#include "ppi/dfa_hopcroft.hpp"
#include "ppi/dfa_const_virtuals.hpp"
#include "ppi/dfa_const_methods_use_walk.hpp"
#include "ppi/post_order.hpp"
#include "ppi/adfa_minimize.hpp"
#include "ppi/path_zip.hpp"
#include "ppi/dfa_mutation_virtuals_basic.hpp"
#include "ppi/dfa_mutation_virtuals_extra.hpp"
#include "ppi/dfa_methods_calling_swap.hpp"
#include "ppi/dfa_set_op.hpp"
#include "ppi/pool_dfa_mmap.hpp"
#include "ppi/state_move_fast.hpp"
//#include "ppi/for_each_word.hpp"
#include "ppi/tpl_for_each_word.hpp"
#include "ppi/tpl_match_key.hpp"
};
template<class StateID, int Sigma, class State>
const StateID DenseDFA<StateID, Sigma, State>::nil_state;
template<class StateID, int Sigma, class State>
const StateID DenseDFA<StateID, Sigma, State>::max_state;

typedef DenseDFA<uint32_t, 320> DenseDFA_uint32_320;
typedef DenseDFA<uint64_t, 320> DenseDFA_uint64_320;

template<class StateID, int Sigma, class State>
size_t
DenseDFA<StateID, Sigma, State>::hopcroft_hash(size_t x_id) const {
	assert(x_id < states.size());
	const State& x = states[x_id];
	assert(!x.is_pzip());
	size_t h = 123;
	for (size_t i = 0; i < MyBitMap::BlockN; ++i) {
		bm_uint_t d = x. dense.block(i);
		bm_uint_t s = x.sparse.block(i);
		h = FaboHashCombine(h, d);
		h = FaboHashCombine(h, s);
	}
	return h;
}

template<class StateID, int Sigma, class State>
size_t
DenseDFA<StateID, Sigma, State>::onfly_hash_hash(size_t x_id) const {
	assert(x_id < states.size());
	const State& x = states[x_id];
	assert(!x.is_pzip());
	size_t h = x.is_term();
	size_t n_sparse = 0;
	for (size_t i = 0; i < MyBitMap::BlockN; ++i) {
		bm_uint_t d = x. dense.block(i);
		bm_uint_t s = x.sparse.block(i);
		h = FaboHashCombine(h, d);
		h = FaboHashCombine(h, s);
		n_sparse += fast_popcount((d ^ s) & s);
	}
	h = FaboHashCombine(h, x.dense_target1);
	h = FaboHashCombine(h, x.dense_target2);
	if (n_sparse) {
		assert(nil_state != x.m_pos);
		size_t pos = x.getpos();
		const StateID* p = &pool.template at<StateID>(pos);
		for (size_t i = 0; i < n_sparse; ++i)
			h = FaboHashCombine(h, p[i]);
		h = FaboHashCombine(h, n_sparse);
	}
	return h;
}

template<class StateID, int Sigma, class State>
bool
DenseDFA<StateID, Sigma, State>::
onfly_hash_equal(size_t x_id, size_t y_id) const {
	assert(x_id < states.size());
	assert(y_id < states.size());
	const State& x = states[x_id];
	const State& y = states[y_id];
	if (x.dense_target1 != y.dense_target1) return false;
	if (x.dense_target2 != y.dense_target2) return false;
	if (x.is_term() != y.is_term()) return false;
	size_t n_sparse = 0;
	for (size_t i = 0; i < MyBitMap::BlockN; ++i) {
		bm_uint_t dx = x. dense.block(i);
		bm_uint_t sx = x.sparse.block(i);
		bm_uint_t dy = y. dense.block(i);
		bm_uint_t sy = y.sparse.block(i);
		if (dx != dy || sx != sy) return false;
		n_sparse += fast_popcount((dx ^ sx) & sx);
	}
	if (n_sparse) {
		assert(nil_state != x.m_pos);
		assert(nil_state != y.m_pos);
		const StateID* px = &pool.template at<StateID>(x.getpos());
		const StateID* py = &pool.template at<StateID>(y.getpos());
		for (size_t i = 0; i < n_sparse; ++i)
			if (px[i] != py[i]) return false;
	}
	return true;
}

template<class StateID, int Sigma, class State>
size_t
DenseDFA<StateID, Sigma, State>::
adfa_hash_hash(const StateID* Min, size_t state_id)
const {
	assert(state_id < states.size());
	const State& x = states[state_id];
	assert(!x.is_pzip());
	size_t h = x.is_term();
	size_t n_sparse = 0;
	for (size_t i = 0; i < MyBitMap::BlockN; ++i) {
		bm_uint_t d = x. dense.block(i);
		bm_uint_t s = x.sparse.block(i);
		h = FaboHashCombine(h, d);
		h = FaboHashCombine(h, s);
		n_sparse += fast_popcount((d ^ s) & s);
	}
	if (nil_state != x.dense_target1)
		h = FaboHashCombine(h, Min[x.dense_target1]);
	if (nil_state != x.dense_target2)
		h = FaboHashCombine(h, Min[x.dense_target2]);
	if (n_sparse) {
		const byte_t* pool = this->pool.data();
		assert(NULL != pool);
		assert(nil_state != x.m_pos);
		size_t pos = x.getpos();
		auto p = reinterpret_cast<const StateID*>(pool + pos);
		for (size_t i = 0; i < n_sparse; ++i)
			h = FaboHashCombine(h, Min[p[i]]);
		h = FaboHashCombine(h, n_sparse);
	}
	return h;
}

template<class StateID, int Sigma, class State>
bool
DenseDFA<StateID, Sigma, State>::
adfa_hash_equal(const StateID* Min, size_t x_id, size_t y_id)
const {
	assert(x_id < states.size());
	assert(y_id < states.size());
	const State& x = states[x_id];
	const State& y = states[y_id];
	size_t n_sparse = 0;
	for (size_t i = 0; i < MyBitMap::BlockN; ++i) {
		bm_uint_t dx = x. dense.block(i);
		bm_uint_t sx = x.sparse.block(i);
		bm_uint_t dy = y. dense.block(i);
		bm_uint_t sy = y.sparse.block(i);
		if (dx != dy || sx != sy) return false;
		n_sparse += fast_popcount((dx ^ sx) & sx);
	}
	if (x.is_term() != y.is_term()) return false;
	if (x.dense_target1 != y.dense_target1) {
		if (nil_state == x.dense_target1) return false;
		if (nil_state == y.dense_target1) return false;
		if (Min[x.dense_target1] != Min[y.dense_target1]) return false;
	}
	if (x.dense_target2 != y.dense_target2) {
		if (nil_state == x.dense_target2) return false;
		if (nil_state == y.dense_target2) return false;
		if (Min[x.dense_target2] != Min[y.dense_target2]) return false;
	}
	if (n_sparse) {
		assert(nil_state != x.m_pos);
		assert(nil_state != y.m_pos);
		const byte_t* pool = this->pool.data();
		assert(NULL != pool);
		auto px = reinterpret_cast<const StateID*>(pool + x.getpos());
		auto py = reinterpret_cast<const StateID*>(pool + y.getpos());
		for (size_t i = 0; i < n_sparse; ++i) {
			StateID ix = px[i];
			StateID iy = py[i];
			//  ix == iy could reduce two indirection through Min[*]
			if (ix != iy && Min[ix] != Min[iy]) return false;
		}
	}
	return true;
}

} // namespace terark
