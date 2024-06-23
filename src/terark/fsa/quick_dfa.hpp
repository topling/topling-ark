#pragma once

#include <terark/bitfield_array.hpp>
#include <terark/num_to_str.hpp>
#include <terark/valvec.hpp>
#include <boost/mpl/if.hpp>
#include "dfa_algo.hpp"
#include "dfa_mmap_header.hpp"
#include "graph_walker.hpp"

#define LargeSigma 450
#define MyType     QuickDFA

namespace terark {

template<int StateBytes, int Sigma = 256>
class QuickDFA : public MatchingDFA {

typedef uint64_t BmUint; // for build_from_aux
#include "ppi/linear_dfa_inclass.hpp"
#include "ppi/quick_dfa_common.hpp"

private:
	static int bmbytes(int bn64) {
		assert(bn64 >= 1);
		assert(bn64 <= 7);
		return sizeof(uint64_t) * bn64;
	}

	static int bmtrans_num(int bn64, const byte_t* bmbeg) {
		assert(bn64 >= 1);
		assert(bn64 <= 7);
		int cnt = 0;
		for(int j = 0; j < bn64; ++j)
			cnt += fast_popcount(unaligned_load<uint64_t>(bmbeg, j));
		return cnt;
	}

	static int bn64_of_bits(int bits) {
		return (bits + 63) / 64;
	}

public:
	size_t num_children(state_id_t curr) const {
		assert(curr < states.size());
		size_t num = 0;
		const State* lstates = states.data();
		state_id_t dest = lstates[curr].u.s.dest;
		if (nil_state != dest) num++;
		if (lstates[curr].u.s.last_bit) return num;
		num++;
		int bn64 = lstates[curr+1].u.t.bn64;
		if (0 == bn64) return 0;
		auto bmbeg = lstates[curr+2].u.z.data;
		for(int j = 0; j < bn64; ++j) {
			uint64_t bm = unaligned_load<uint64_t>(bmbeg, j);
			num += fast_popcount(bm);
		}
		return num;
	}

	template<class OP>
	void for_each_move(state_id_t curr, OP op) const {
		assert(curr < states.size());
		const State* lstates = states.data();
		state_id_t dest = lstates[curr].u.s.dest;
		if (nil_state != dest) op(dest, lstates[curr].u.s.ch);
		if (lstates[curr].u.s.last_bit) return;
		op(lstates[curr+1].u.t.dest, lstates[curr+1].u.t.ch);
		int bn64 = lstates[curr+1].u.t.bn64;
		if (0 == bn64) return;
		auto minch = lstates[curr+1].u.t.ch+1;
		auto bmbeg = lstates[curr+2].u.z.data;
		auto trans = bmbeg + bmbytes(bn64);
		int k = 0;
		for(int j = 0; j < bn64; ++j) {
			uint64_t bm = unaligned_load<uint64_t>(bmbeg, j);
			auchar_t ch = minch + j * 64;
			while (bm) {
				int ctz = fast_ctz(bm);
				ch += ctz;
				dest = trans_array::unaligned_get0(trans, k++);
				op(dest, ch);
				ch++;
				bm >>= ctz;
				bm >>= 1;
			}
		}
	}

	state_id_t state_move(size_t curr, auchar_t ch) const {
		assert(ch	< Sigma);
		assert(curr < states.size());
		const State* lstates = states.data();
		if (lstates[curr].u.s.ch == ch) return lstates[curr].u.s.dest;
		if (lstates[curr].u.s.last_bit) return nil_state;
		if (ch	== lstates[curr+1].u.t.ch) return lstates[curr+1].u.t.dest;
		int bn64 = lstates[curr+1].u.t.bn64;
		if (0 == bn64) return nil_state;
		auto minch = lstates[curr+1].u.t.ch + 1;
		int bidx = ch - minch;
		if (bidx < 64*bn64) {
			auto bmbeg = lstates[curr+2].u.z.data;
			auto bmhit = unaligned_load<uint64_t>(bmbeg, bidx/64);
			if (bmhit & uint64_t(1) << bidx%64) {
				auto trans = bmbeg + sizeof(uint64_t) * bn64;
				int  tidx = 0;
				for(int j = 0; j < bidx/64; ++j)
					tidx += fast_popcount(unaligned_load<uint64_t>(bmbeg, j));
				if (bidx%64)
					tidx += fast_popcount_trail(bmhit, bidx%64);
				return trans_array::unaligned_get0(trans, tidx);
			}
		}
		return nil_state;
	}
};

#include "ppi/linear_dfa_typedef.hpp"

//////////////////////////////////////////////////////////////////////////////

} // namespace terark
