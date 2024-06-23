#pragma once

#include <terark/num_to_str.hpp>
#include <terark/valvec.hpp>
#include <boost/mpl/if.hpp>
#include "dfa_algo.hpp"
#include "dfa_mmap_header.hpp"
#include "graph_walker.hpp"

namespace terark {

#define LargeSigma 512
#define MyType     LinearDFA

template<int StateBytes, int Sigma = 256>
class LinearDFA : public MatchingDFA {

#include "ppi/linear_dfa_inclass.hpp"

	size_t num_children(state_id_t curr) const {
		size_t num = 0;
		for_each_dest(curr, [&](state_id_t){num++;});
		return num;
	}

	template<class OP>
	void for_each_dest(state_id_t curr, OP op) const {
		assert(curr < states.size());
		const State* lstates = states.data();
		state_id_t dest = lstates[curr].u.s.dest;
		if (nil_state != dest) op(dest);
		if (lstates[curr].u.s.last_bit) return;
		size_t i = curr + (lstates[curr].u.s.pzip_bit ? zpath_slots(curr) : 0);
		do { ++i;
			assert(i < states.size());
			assert(lstates[i].u.s.dest < states.size());
			op(lstates[i].u.s.dest);
		} while (!lstates[i].u.s.last_bit);
	}

	template<class OP>
	void for_each_dest_rev(state_id_t curr, OP op) const {
		assert(curr < states.size());
		const State* lstates = states.data();
		if (lstates[curr].u.s.last_bit) {
			state_id_t dest = lstates[curr].u.s.dest;
			if (nil_state != dest) op(dest);
			return;
		}
		size_t i = curr + (lstates[curr].u.s.pzip_bit ? zpath_slots(curr) : 0);
		size_t n = 1;
		do { ++i; ++n;
			assert(i < states.size());
		} while (!lstates[i].u.s.last_bit);
		if (lstates[curr].u.s.pzip_bit) {
			for(size_t j = 0; j < n-1; ++j) {
				size_t k = i - j;
				state_id_t dest = lstates[k].u.s.dest;
				assert(nil_state != dest);
				op(dest);
			}
			state_id_t dest = lstates[curr].u.s.dest;
			assert(nil_state != dest);
			op(dest);
		}
		else {
			for(size_t j = 0; j < n; ++j) {
				size_t k = i - j;
				state_id_t dest = lstates[k].u.s.dest;
				assert(nil_state != dest);
				op(dest);
			}
		}
	}

	template<class OP>
	void for_each_move(state_id_t curr, OP op) const {
		assert(curr < states.size());
		const State* lstates = states.data();
		state_id_t dest = lstates[curr].u.s.dest;
		if (nil_state != dest) op(dest, lstates[curr].u.s.ch);
		if (lstates[curr].u.s.last_bit) return;
		size_t i = curr + (lstates[curr].u.s.pzip_bit ? zpath_slots(curr) : 0);
		do { ++i;
			assert(i < states.size());
			assert(lstates[i].u.s.dest < states.size());
			op(lstates[i].u.s.dest, lstates[i].u.s.ch);
		} while (!lstates[i].u.s.last_bit);
	}

	state_id_t state_move(state_id_t curr, auchar_t ch) const {
		assert(ch   < Sigma);
		assert(curr < states.size());
		const State* lstates = states.data();
		if (lstates[curr].u.s.ch == ch) return lstates[curr].u.s.dest;
		if (lstates[curr].u.s.last_bit) return nil_state;
		size_t i = curr + (lstates[curr].u.s.pzip_bit ? zpath_slots(curr) : 0);
		do { ++i;
			assert(i < states.size());
			if (ch == lstates[i].u.s.ch)
				return lstates[i].u.s.dest;
		} while (!lstates[i].u.s.last_bit);
		return nil_state;
	}
	fstring get_zpath_data(size_t s, MatchContext*) const {
		assert(s+1 < states.size());
		assert(states[s].u.s.pzip_bit);
		const unsigned char* p = states.ref(s+1).u.z.data;
#ifndef NDEBUG
		size_t zlen = *p;
		size_t zp_slots = slots_of_bytes(zlen+1);
		assert(s + zp_slots < states.size());
#endif
	//	printf("MyType::get_zpath_data(%zd) = %02X: %.*s\n", s, *p, *p, p+1);
		return fstring(p+1, *p);
	}
private:
	size_t zpath_slots(size_t s) const {
		size_t zlen = this->get_zpath_data(s, NULL).size();
		return slots_of_bytes(zlen+1);
	}
	template<class Walker, class Au>
	void build_from_aux(const Au& au, typename Au::state_id_t root) {
		typedef typename Au::state_id_t au_state_id_t;
		valvec<au_state_id_t> map;
		map.resize_no_init(au.total_states());
		size_t idx = 0;
		Walker walker;
		walker.resize(au.total_states());
		walker.putRoot(root);
		while (!walker.is_finished()) {
			au_state_id_t curr = walker.next();
			map[curr] = idx;
			size_t n_child = au.num_children(curr);
			if (au.is_pzip(curr)) {
				size_t zlen = au.get_zpath_data(curr, NULL).size();
				idx += slots_of_bytes(zlen+1);
			}
			idx += (0 == n_child) ? 1 : n_child;
			walker.putChildren(&au, curr);
		}
		if (idx >= max_state) {
			throw std::out_of_range("LinearDFA::build_from_aux: idx exceeds max_state");
		}
		states.resize_no_init(idx);
		walker.resize(au.total_states());
		walker.putRoot(root);
		idx = 0;
		State* lstates = states.data();
		while (!walker.is_finished()) {
			au_state_id_t curr = walker.next();
			size_t j = 0;
			bool b_is_term = au.is_term(curr);
			assert(idx == map[curr]);
			if (au.is_pzip(curr)) {
				const unsigned char* zd = au.get_zpath_data(curr, NULL).udata()-1;
				int zlen = *zd;
				size_t zp_slots = slots_of_bytes(zlen+1);
				au.for_each_move(curr,
					[&](au_state_id_t dst, auchar_t ch) {
						assert(dst < map.size());
						assert(idx+j < states.size());
						lstates[idx+j].u.s.last_bit = 0;
						lstates[idx+j].u.s.pzip_bit = 1;
						lstates[idx+j].u.s.term_bit = b_is_term;
						lstates[idx+j].u.s.ch = ch;
						lstates[idx+j].u.s.dest = map[dst];
						if (0 == j) {
						//	printf("1: j=%zd zlen+1=%d zp_slots=%zd : %.*s\n", j, zlen+1, zp_slots, zlen, zd+1);
							memcpy(lstates[idx+1].u.z.data, zd, zlen+1);
							j = 1 + zp_slots;
						} else
							j++;
					});
				if (0 == j) {
				//	printf("2: j=%zd zlen+1=%d zp_slots=%zd : %.*s\n", j, zlen+1, zp_slots, zlen, zd+1);
					lstates[idx].u.s.last_bit = 1;
					lstates[idx].u.s.pzip_bit = 1;
					lstates[idx].u.s.term_bit = b_is_term;
					lstates[idx].u.s.ch = 0;
					lstates[idx].u.s.dest = nil_state;
					memcpy(lstates[idx+1].u.z.data, zd, zlen+1);
					j = 1 + zp_slots;
				}
				else if (1 + zp_slots == j)
					lstates[idx].u.s.last_bit = 1; // last edge
				else
					lstates[idx+j-1].u.s.last_bit = 1; // last edge
			}
			else {
				au.for_each_move(curr,
					[&](au_state_id_t dst, auchar_t ch) {
						assert(dst < map.size());
						assert(idx+j < states.size());
						lstates[idx+j].u.s.last_bit = 0;
						lstates[idx+j].u.s.pzip_bit = 0;
						lstates[idx+j].u.s.term_bit = b_is_term;
						lstates[idx+j].u.s.ch = ch;
						lstates[idx+j].u.s.dest = map[dst];
						j++;
					});
				if (0 == j) {
					lstates[idx].u.s.last_bit = 1;
					lstates[idx].u.s.pzip_bit = 0;
					lstates[idx].u.s.term_bit = b_is_term;
					lstates[idx].u.s.ch = 0;
					lstates[idx].u.s.dest = nil_state;
					j = 1;
				} else
					lstates[idx+j-1].u.s.last_bit = 1; // last edge
			}
			idx += j;
			walker.putChildren(&au, curr);
		}
		m_zpath_states = au.num_zpath_states();
		assert(states.size() == idx);
	}
};

#include "ppi/linear_dfa_typedef.hpp"

} // namespace terark


//////////////////////////////////////////////////////////////////////////////
