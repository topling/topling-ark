#pragma once

#include <terark/bitfield_array.hpp>
#include <terark/num_to_str.hpp>
#include <terark/valvec.hpp>
#include <boost/mpl/if.hpp>
#include "dfa_algo.hpp"
#include "dfa_mmap_header.hpp"
#include "graph_walker.hpp"

#define LargeSigma 320
#define MyType	   SQuickDFA

namespace terark {

template<int StateBytes, int Sigma = 256>
class SQuickDFA : public MatchingDFA {

typedef uint32_t BmUint; // for build_from_aux
#include "ppi/linear_dfa_inclass.hpp"
#include "ppi/quick_dfa_common.hpp"

private:
	static int bmbytes(int bn64) {
		assert(bn64 >= 1);
		assert(bn64 <= 7);
		using namespace std; // for min and max
		if (bn64 <= 4)
			return sizeof(uint32_t) * bn64;
		else
			return sizeof(uint32_t) * 4 + sizeof(uint64_t) * (bn64 - 4);
	}

	static int bmtrans_num(int bn64, const byte_t* bmbeg) {
		assert(bn64 >= 1);
		assert(bn64 <= 7);
		using namespace std; // for min and max
		int cnt = 0;
		for(int j = 0; j < min(bn64, 4); ++j)
			cnt += fast_popcount(unaligned_load<uint32_t>(bmbeg, j));
		for(int j = 0; j < max(bn64-4, 0); ++j) // sizeof(uint32_t)=4
			cnt += fast_popcount(unaligned_load<uint64_t>(bmbeg + 4*4, j));
		return cnt;
	}

	static int bn64_of_bits(int bits) {
		return (bits <= 32*4)
			 ? (bits + 31) / 32
			 : (bits + 63 - 32*4) / 64 + 4
			 ;
	}

public:
	size_t num_children(state_id_t curr) const {
		size_t num = 0;
		assert(curr < states.size());
		const State* lstates = states.data();
		state_id_t dest = lstates[curr].u.s.dest;
		if (nil_state != dest) num++;
		if (lstates[curr].u.s.last_bit) return num;
		num++;
		int bn64 = lstates[curr+1].u.t.bn64;
		if (0 == bn64) return num;
		auto bmbeg = lstates[curr+2].u.z.data;
		using namespace std; // for min & max
		for(int j = 0; j < min(bn64, 4); ++j) {
			uint32_t bm = unaligned_load<uint32_t>(bmbeg, j);
			num += fast_popcount(bm);
		}
		if (bn64 <= 4) return num;
		bmbeg += sizeof(uint32_t) * 4;
		bn64 -= 4;
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
		using namespace std; // for min & max
		for(int j = 0; j < min(bn64, 4); ++j) {
			uint32_t bm = unaligned_load<uint32_t>(bmbeg, j);
			auchar_t ch = minch + j * 32;
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
		if (bn64 <= 4) return;
		bmbeg += sizeof(uint32_t) * 4;
		minch += 32 * 4;
		bn64 -= 4;
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
		if (bidx < 32*4) {
			if (bidx < 32*bn64) {
				auto bmbeg = lstates[curr+2].u.z.data;
				auto bmhit = unaligned_load<uint32_t>(bmbeg, bidx/32);
				if (bmhit & uint32_t(1) << bidx%32) {
					auto trans = bmbeg + bmbytes(bn64);
					int  tidx = 0;
					for(int j = 0; j < bidx/32; ++j)
						tidx += fast_popcount(unaligned_load<uint32_t>(bmbeg, j));
					if (bidx%32)
						tidx += fast_popcount_trail(bmhit, bidx%32);
					return trans_array::unaligned_get0(trans, tidx);
				}
				return nil_state;
			}
		}
		bn64 -= 2; // 4 uint32_t block is 2 uint64_t block
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

} // namespace terark

//////////////////////////////////////////////////////////////////////////////
