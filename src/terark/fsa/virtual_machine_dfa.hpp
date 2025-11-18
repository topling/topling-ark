#pragma once

#include "dfa_algo.hpp"
#include "mre_delim.hpp"
#include <terark/mempool_lock_none.hpp>

namespace terark {

class VirtualMachineDFA : public BaseDFA {
public:
	static const uint32_t  BitMask32 = 0x55555555UL;
	static const uint64_t  BitMask64 = 0x5555555555555555ULL;

	typedef uint32_t u32;
	typedef  int32_t s32;

	typedef uint32_t state_id_t;
	typedef uint32_t transition_t;
	static const state_id_t nil_state = state_id_t(-1);
	static const state_id_t max_state = state_id_t(-2);

	// VirtualMachineDFA can be viewed as a virtual machine
	// DFA state types can be viewed as the op code of the instruction set
	enum OpCode {
		leaf_final_state , // has a match_id, has no child
		all_char_all_jump, // a.{1000}b with fast jump
		one_char_one_diff, // delta(s, ch) = s + diff_val
		one_char_one_dest,
		all_diff_one_loop, // (.*b) will generate such states
		all_dest_one_loop,
		all_loop_one_diff, // (.*b) will generate such states
		all_loop_one_dest,
		mul_char_one_diff_fullfill, // [a-z]b
		mul_char_one_dest_fullfill,
		neg_char_one_diff_fullfill, // [^a-z]b, [0, minch) (maxch, 256]
		neg_char_one_dest_fullfill,
		mul_char_one_dest_small_bm,
		mul_char_one_dest_large_bm,
		mul_char_two_dest_fullfill, // [a-z]*[a-f], with bitmap
		mul_loop_one_dest_fullfill, // [a-f]*[a-z], with bitmap, loop on 0
		mul_char_2o3_dest_small_bm, // 2 or 3 dest
		mul_char_2o3_dest_large_bm, // 2 or 3 dest
		mul_char_mul_dest_small_bm,
		mul_char_mul_dest_large_bm,
		mul_loop_1o2_dest_small_bm, // 1 or 2 non-loop dest
		mul_loop_1o2_dest_large_bm, // 1 or 2 non-loop dest
		mul_char_mul_dest_huge4_bm, // 4 bits per label
		mul_loop_mul_dest_huge4_bm, // 4 bits per label
//		mul_loop_non_dest_fullfill, // EX: final state of "^a[a-z]*$"
//		mul_loop_non_dest_small_bm, // EX: final state of "^a[0-47-9]*$"
//		mul_loop_non_dest_large_bm, // EX: final state of "^a[0-9a-z]*$"
		mul_dest_index_mapped, // such as ".a", first byte of utf8
		mul_loop_index_mapped,
		mul_dest_index_mapped_i8,
		mul_loop_index_mapped_i8,
		mul_dest_index_mapped_i16,
		mul_loop_index_mapped_i16,
		direct_mapped_i8,
		OpCode_last
	};
	static const char* get_op_name(OpCode op) {
		static const char* names[] = {
		"leaf_final_state" , // has a match_id, has no child
		"all_char_all_jump", // a.{1000}b with fast jump
		"one_char_one_diff", // delta(s, ch) = s + diff_val
		"one_char_one_dest",
		"all_diff_one_loop", // (.*b) will generate such states
		"all_dest_one_loop",
		"all_loop_one_diff", // (.*b) will generate such states
		"all_loop_one_dest",
		"mul_char_one_diff_fullfill", // [a-z]b
		"mul_char_one_dest_fullfill",
		"neg_char_one_diff_fullfill", // [^a-z]b, [0, minch) (maxch, 256]
		"neg_char_one_dest_fullfill",
		"mul_char_one_dest_small_bm",
		"mul_char_one_dest_large_bm",
		"mul_char_two_dest_fullfill", // [a-z]*[a-f], with bitmap
		"mul_loop_one_dest_fullfill", // [a-f]*[a-z], with bitmap, loop on 0
		"mul_char_2o3_dest_small_bm", // 2 or 3 dest
		"mul_char_2o3_dest_large_bm", // 2 or 3 dest
		"mul_char_mul_dest_small_bm",
		"mul_char_mul_dest_large_bm",
		"mul_loop_1o2_dest_small_bm", // 1 or 2 non-loop dest
		"mul_loop_1o2_dest_large_bm", // 1 or 2 non-loop dest
		"mul_char_mul_dest_huge4_bm", // 4 bits per label
		"mul_loop_mul_dest_huge4_bm", // 4 bits per label
//		"mul_loop_non_dest_fullfill", // EX: final state of "^a[a-z]*$"
//		"mul_loop_non_dest_small_bm", // EX: final state of "^a[0-47-9]*$"
//		"mul_loop_non_dest_large_bm", // EX: final state of "^a[0-9a-z]*$"
		"mul_dest_index_mapped",
		"mul_loop_index_mapped",
		"mul_dest_index_mapped_i8",
		"mul_loop_index_mapped_i8",
		"mul_dest_index_mapped_i16",
		"mul_loop_index_mapped_i16",
		"direct_mapped_i8",
		};
		assert(op < sizeof(names)/sizeof(names[0]));
		if (op < sizeof(names)/sizeof(names[0]))
			return names[op];
		else
			return "UnkownOpCode";
	}
#pragma pack(push,1)
#define INST_OP_PZIP_TERM() \
		u32 op       :  5; \
		u32 pzip_bit :  1; \
		u32 term_bit :  1; \
		u32 has_capt :  1

	struct Inst_leaf_final_state {
		INST_OP_PZIP_TERM();
		u32 match_mul:  1;
		u32 match_id : 23;
	};
	struct Inst_all_char_all_jump {
		INST_OP_PZIP_TERM();
		u32 max_jump : 24;
	};
	/// one_char_one_diff
	/// all_diff_one_loop
	/// all_loop_one_diff
	struct Inst_one_char_one_diff {
		INST_OP_PZIP_TERM();
		u32 ch       :  8;
		s32 diff_val : 16;
	};
	/// one_char_one_dest
	/// all_dest_one_loop
	/// all_loop_one_dest
	struct Inst_one_char_one_dest {
		INST_OP_PZIP_TERM();
		u32 ch       :  8;
		u32 match_mul:  1;
		u32 match_id : 15;
		u32 target;
	};
	struct Inst_mul_char_one_diff_fullfill {
		INST_OP_PZIP_TERM();
		u32 minch    : 8;
		u32 maxch    : 8;
		s32 diff_val : 8;
		u32 range() const { return maxch - minch + 1; }
	};
	struct Inst_mul_char_one_dest_fullfill {
		INST_OP_PZIP_TERM();
		u32 minch : 8;
		u32 maxch : 8;
		u32 waste : 8;
		u32 target;
	};
	typedef Inst_mul_char_one_diff_fullfill Inst_neg_char_one_diff_fullfill;
	typedef Inst_mul_char_one_dest_fullfill Inst_neg_char_one_dest_fullfill;

	struct Inst_mul_char_one_dest_small_bm {
		INST_OP_PZIP_TERM();
		u32 minch :  8;
		u32 bits1 : 16;
		u32 target;
	};
	struct Inst_mul_char_one_dest_large_bm {
		INST_OP_PZIP_TERM();
		u32 minch :  8;
		u32 nbm   :  3; // need +1
	   	u32 bits1 : 13;
		u32 tarbits[1];
	};
	struct Inst_mul_char_two_dest_fullfill {
		INST_OP_PZIP_TERM();
		u32 minch : 8;
		u32 maxch : 8; // need +1
	   	u32 bits1 : 8;
		u32 tarbits[1];
		size_t calc_nbm() const { return (maxch-minch+1+31-8)/32; }
	};
	typedef Inst_mul_char_two_dest_fullfill
			Inst_mul_loop_one_dest_fullfill;
	struct Inst_mul_char_2o3_dest_small_bm {
		INST_OP_PZIP_TERM();
		u32 minch :  8;
		u32 bits1 : 16;
		u32 targets[3]; // 2 or 3 or mul
	};
	struct Inst_mul_char_2o3_dest_large_bm {
		INST_OP_PZIP_TERM();
		u32 minch :  8;
		u32 nbm   :  4;
		u32 bits1 : 12;
		u32 tarbits[1];
	};
	struct Inst_mul_char_mul_dest_small_bm {
		INST_OP_PZIP_TERM();
		u32 minch    :  8;
		u32 bits1    : 16;
		u32 target1;
		u32 target2;
		u32 targets[1];
	};
	struct Inst_mul_char_mul_dest_large_bm {
		INST_OP_PZIP_TERM();
		u32 r_sum : 8; // == max rank + 1
		u32 maxch : 8;
		u32 minch : 8;
		u32 target1;
		u32 target2;
		uint08_t rank[8];
		union {
			uint32_t bm32[1];
			uint64_t bm64[1];
		} ubits; // other targets are following ubits
	};
	typedef Inst_mul_char_2o3_dest_small_bm
			Inst_mul_loop_1o2_dest_small_bm;
	typedef Inst_mul_char_2o3_dest_large_bm
			Inst_mul_loop_1o2_dest_large_bm;
#if 0
	struct Inst_mul_loop_non_dest_fullfill {
		INST_OP_PZIP_TERM();
		u32 minch : 8;
		u32 maxch : 8;
	   	u32 waste : 8;
	};
	struct Inst_mul_loop_non_dest_small_bm {
		INST_OP_PZIP_TERM();
		u32 minch :  8;
	   	u32 bits1 : 16;
	};
	struct Inst_mul_loop_non_dest_large_bm {
		INST_OP_PZIP_TERM();
		u32 minch :  8;
		u32 nbm   :  3; // need +1
	   	u32 bits1 : 13;
		u32 bitvec[1];
	};
#endif
	struct Inst_mul_char_mul_dest_huge4_bm {
		INST_OP_PZIP_TERM();
		u32 minch : 8;
		u32 maxch : 8;
		u32 nUniq : 8;
		u32 tarbits[1];
	};
	typedef Inst_mul_char_mul_dest_huge4_bm
			Inst_mul_loop_mul_dest_huge4_bm;
	struct BaseInst_index_mapped {
		INST_OP_PZIP_TERM();
		u32 map_id : 24;
	};
	struct Inst_mul_dest_index_mapped {
		INST_OP_PZIP_TERM();
		u32 map_id : 24;
		u32 targets[1];
	};
	typedef Inst_mul_dest_index_mapped
			Inst_mul_loop_index_mapped;
	struct Inst_mul_dest_index_mapped_i8 {
		INST_OP_PZIP_TERM();
		u32 map_id : 24;
		int8_t diffs_i8[1];
	};
	typedef Inst_mul_dest_index_mapped_i8
			Inst_mul_loop_index_mapped_i8;
	struct Inst_mul_dest_index_mapped_i16 {
		INST_OP_PZIP_TERM();
		u32 map_id : 24;
		int16_t diffs_i16[1];
	};
	typedef Inst_mul_dest_index_mapped_i16
			Inst_mul_loop_index_mapped_i16;
#pragma pack(pop)
	typedef Inst_one_char_one_diff State, state_t;
	BOOST_STATIC_ASSERT(4 == sizeof(State));

	static const size_t bmbits = sizeof(bm_uint_t) * 8;
	static const bm_uint_t one = bm_uint_t(1);
	enum { sigma = 256 };

	VirtualMachineDFA() {
		m_dyn_sigma = 256;
		m_gnode_states = 0;
		m_atom_dfa_num = 0;
		m_dfa_cluster_num = 0;
		m_transition_num = 0;
	}
	~VirtualMachineDFA() {
		if (this->mmap_base) {
			states.risk_release_ownership();
			m_roots.risk_release_ownership();
			m_index_map.risk_release_ownership();
		}
	}

	bool is_term(size_t state) const {
		assert(state < states.size());
		return states[state].term_bit;
	}
	bool is_pzip(size_t state) const {
		assert(state < states.size());
		return states[state].pzip_bit;
	}

	void clear() {
		states.clear();
		m_zpath_states = 0;
		m_total_zpath_len = 0;
		m_transition_num = 0;
	}
	void erase_all() {
		states.erase_all();
		m_zpath_states = 0;
		m_total_zpath_len = 0;
		m_transition_num = 0;
	}
	void swap(VirtualMachineDFA& y) {
		BaseDFA::risk_swap(y);
		states.swap(y.states);
		m_roots.swap(y.m_roots);
		std::swap(m_gnode_states, y.m_gnode_states);
	}

	bool has_freelist() const override final { return false; }

	size_t mem_size() const override { return sizeof(state_t)*states.size(); }
	size_t total_states() const { return states.size(); }
	size_t num_used_states() const { return states.size(); }
	size_t num_free_states() const { return 0; }
	size_t v_gnode_states()  const override final { return m_gnode_states; }
	size_t total_transitions() const { return m_transition_num; }

	void resize_states(size_t newsize) {
		assert(newsize < max_state);
	   	states.resize(newsize);
   	}

	size_t v_num_children(size_t) const override { TERARK_DIE("should not call"); }

	bool v_has_children(size_t s) const override final {
		return states[s].op != leaf_final_state;
	}

	bool is_free(size_t s) const {
		assert(s < states.size());
		return false;
	}

	bool is_dead(size_t s) const {
		return states[s].op == leaf_final_state && !states[s].term_bit;
	}

	/// not include zpath, match_id, caputure
	size_t instruction_slots(const State* u) const {
		switch (OpCode(u->op)) {
		default:
			assert(0);
			THROW_STD(logic_error, "invalid OpCode = %d", u->op);
		case leaf_final_state:
		//	assert(1 == u->term_bit);
			return 1;
		case all_char_all_jump:
		case one_char_one_diff:
			return 1;
		case one_char_one_dest:
			return 2;
		case all_diff_one_loop:
		case all_loop_one_diff:
			return 1;
		case mul_char_one_diff_fullfill:
		case neg_char_one_diff_fullfill:
			return 1;
		case mul_char_one_dest_fullfill:
		case neg_char_one_dest_fullfill:
			return 2;
		case mul_char_one_dest_small_bm:
			return 2;
		case mul_char_one_dest_large_bm:
			return 3 + ((const Inst_mul_char_one_dest_large_bm*)(u))->nbm;
		case mul_char_two_dest_fullfill:
			return 3 + ((const Inst_mul_char_two_dest_fullfill*)(u))->calc_nbm();
		case mul_loop_one_dest_fullfill:
			return 2 + ((const Inst_mul_loop_one_dest_fullfill*)(u))->calc_nbm();
		case mul_char_2o3_dest_small_bm:
		  {
			auto p = (const Inst_mul_char_2o3_dest_small_bm*)(u);
			unsigned b = p->bits1;
			return (b & (b >> 1) & BitMask32) ? 4 : 3;
		  }
		case mul_char_2o3_dest_large_bm:
		  {
			auto p = (const Inst_mul_char_2o3_dest_large_bm*)(u);
			unsigned nbm = p->nbm + 1;
			{
				unsigned b = p->bits1;
				if (b & (b >> 1) & BitMask32)
					return 1 + nbm + 3;
			}
			for(unsigned i = 0; i < nbm; ++i) {
				uint32_t b = p->tarbits[i];
				if (b & (b >> 1) & BitMask32)
					return 1 + nbm + 3;
			}
			return 1 + nbm + 2;
		  }
		case mul_char_mul_dest_small_bm:
		  {
			auto p = (const Inst_mul_char_mul_dest_small_bm*)(u);
			unsigned b = p->bits1;
			return 3 + fast_popcount32(b & (b >> 1) & BitMask32);
		  }
		case mul_char_mul_dest_large_bm:
		  {
			auto p = (const Inst_mul_char_mul_dest_large_bm*)(u);
			return 5 + p->r_sum + 1 + (2*(p->maxch-p->minch+1)+63)/64 * 2;
		  }
		case mul_loop_1o2_dest_small_bm:
		  {
			auto p = (const Inst_mul_loop_1o2_dest_small_bm*)(u);
			unsigned b = p->bits1;
			return (b & (b >> 1) & BitMask32) ? 3 : 2;
		  }
		case mul_loop_1o2_dest_large_bm:
		  {
			auto p = (const Inst_mul_loop_1o2_dest_large_bm*)(u);
			unsigned nbm = p->nbm + 1;
			{
				unsigned b = p->bits1;
				if (b & (b >> 1) & BitMask32)
					return 1 + nbm + 2;
			}
			for(unsigned i = 0; i < nbm; ++i) {
				uint32_t b = p->tarbits[i];
				if (b & (b >> 1) & BitMask32)
					return 1 + nbm + 2;
			}
			return 1 + nbm + 1;
		  }
		case mul_char_mul_dest_huge4_bm:
		case mul_loop_mul_dest_huge4_bm: // nUniq doesn't include loop target
		  {
			auto p = (const Inst_mul_char_mul_dest_huge4_bm*)(u);
			size_t nbm = ((p->maxch - p->minch + 1) * 4 + 31) / 32;
			return 1 + nbm + p->nUniq;
		  }
//		case mul_loop_non_dest_fullfill:
//			return 1;
//		case mul_loop_non_dest_small_bm:
//			return 1;
//		case mul_loop_non_dest_large_bm:
//			return 2 + ((const Inst_mul_loop_non_dest_large_bm*)(u))->nbm;
		case all_dest_one_loop:
		case all_loop_one_dest:
			return 2;
		case mul_dest_index_mapped:
		  {
			auto p = ((const Inst_mul_dest_index_mapped*)(u));
			return 1 + m_index_map[p->map_id].n_uniq;
		  }
		case mul_loop_index_mapped:
		  {
			auto p = ((const Inst_mul_dest_index_mapped*)(u));
			return m_index_map[p->map_id].n_uniq; // n_uniq include the loop
		  }
		case mul_dest_index_mapped_i8:
		  {
			auto p = ((const Inst_mul_dest_index_mapped*)(u));
			return 1 + (m_index_map[p->map_id].n_uniq + 3) / 4;
		  }
		case mul_loop_index_mapped_i8:
		  {
			auto p = ((const Inst_mul_dest_index_mapped*)(u));
			return 1 + (m_index_map[p->map_id].n_uniq + 2) / 4; // n_uniq include the loop
		  }
		case mul_dest_index_mapped_i16:
		  {
			auto p = ((const Inst_mul_dest_index_mapped*)(u));
			return 1 + (m_index_map[p->map_id].n_uniq + 1) / 2;
		  }
		case mul_loop_index_mapped_i16:
		  {
			auto p = ((const Inst_mul_dest_index_mapped*)(u));
			return 1 + (m_index_map[p->map_id].n_uniq + 0) / 2; // n_uniq include the loop
		  }
		case direct_mapped_i8:
			return 1;
		}
		assert(0); // unreachable
		return 0;
	}

	fstring get_zpath_data(size_t s, MatchContext*) const {
		assert(s < states.size());
		assert(states[s].pzip_bit);
		const State* u = states.data() + s;
		auto i = instruction_slots(u);
		auto p = reinterpret_cast<const byte_t*>(u + i);
		assert(*p);
		return fstring(p + 1, *p);
	}

	template<class Collector>
	size_t read_matchid(size_t state, Collector* vec) const {
		assert(state < states.size());
		const State* u = states.data() + state;
		assert(u->term_bit);
		size_t slots = instruction_slots(u);
		if (u->pzip_bit) {
			auto zp = reinterpret_cast<const byte_t*>(u + slots);
			slots += (zp[0] + 1 + 3) / 4;
		}
		switch (OpCode(u->op)) {
		default:
			assert(0);
			THROW_STD(logic_error, "invalid OpCode = %d", u->op);
		case leaf_final_state:
		  {
			assert(u->term_bit || initial_state == state);
			auto p = reinterpret_cast<const Inst_leaf_final_state*>(u);
			if (p->match_mul) {
				size_t cnt = p->match_id;
				vec->append(reinterpret_cast<const s32*>(u + slots), cnt);
				return cnt;
			}
			vec->push(p->match_id);
			return 1;
		  }
		case one_char_one_dest:
		case all_dest_one_loop:
		case all_loop_one_dest:
		  {
			auto p = reinterpret_cast<const Inst_one_char_one_dest*>(u);
			if (p->match_mul) {
				size_t cnt = p->match_id;
				vec->append(reinterpret_cast<const s32*>(u + slots), cnt);
				return cnt;
			}
			vec->push(p->match_id);
			return 1;
		  }
		case all_char_all_jump:
		case one_char_one_diff:
		case all_diff_one_loop:
		case all_loop_one_diff:
		case mul_char_one_diff_fullfill:
		case mul_char_one_dest_fullfill:
		case neg_char_one_diff_fullfill:
		case neg_char_one_dest_fullfill:
		case mul_char_one_dest_small_bm:
		case mul_char_one_dest_large_bm:
		case mul_char_two_dest_fullfill:
		case mul_loop_one_dest_fullfill:
		case mul_char_2o3_dest_small_bm:
		case mul_char_2o3_dest_large_bm:
		case mul_char_mul_dest_small_bm:
		case mul_char_mul_dest_large_bm:
		case mul_loop_1o2_dest_small_bm:
		case mul_loop_1o2_dest_large_bm:
		case mul_char_mul_dest_huge4_bm:
		case mul_loop_mul_dest_huge4_bm: // nUniq doesn't include loop target
//		case mul_loop_non_dest_fullfill:
//		case mul_loop_non_dest_small_bm:
//		case mul_loop_non_dest_large_bm:
		case mul_dest_index_mapped:
		case mul_loop_index_mapped:
		case mul_dest_index_mapped_i8:
		case mul_loop_index_mapped_i8:
		case mul_dest_index_mapped_i16:
		case mul_loop_index_mapped_i16:
		case direct_mapped_i8:
			if (reinterpret_cast<const s32*>(u)[slots] < 0) {
				size_t cnt = -reinterpret_cast<const s32*>(u)[slots];
				assert(cnt + state < states.size());
				vec->append(reinterpret_cast<const s32*>(u) + slots + 1, cnt);
				return cnt;
			}
			else {
				vec->push(reinterpret_cast<const s32*>(u)[slots]);
				return 1;
			}
		}
		assert(0); // unreachable
		return 0;
	}

	bool has_capture(size_t state) const {
		assert(state < states.size());
		return states[state].has_capt;
	}

	const byte_t* get_capture(size_t state) const {
		assert(state < states.size());
		const State* u = states.data() + state;
		assert(u->has_capt);
		size_t slots = instruction_slots(u);
		if (u->pzip_bit) {
			auto zp = reinterpret_cast<const byte_t*>(u + slots);
			slots += (zp[0] + 1 + 3) / 4;
		}
		switch (OpCode(u->op)) {
		default:
			assert(0);
			THROW_STD(logic_error, "invalid OpCode = %d", u->op);
		case leaf_final_state:
		  {
			assert(u->term_bit || initial_state == state);
			auto p = reinterpret_cast<const Inst_leaf_final_state*>(u);
			if (p->match_mul)
				slots += p->match_id; // matchid_cnt
			break;
		  }
		case one_char_one_dest:
		case all_dest_one_loop:
		case all_loop_one_dest:
		  {
			auto p = reinterpret_cast<const Inst_one_char_one_dest*>(u);
			if (p->match_mul)
				slots += p->match_id; // matchid_cnt
			break;
		  }
		case all_char_all_jump:
		case one_char_one_diff:
		case all_diff_one_loop:
		case all_loop_one_diff:
		case mul_char_one_diff_fullfill:
		case mul_char_one_dest_fullfill:
		case neg_char_one_diff_fullfill:
		case neg_char_one_dest_fullfill:
		case mul_char_one_dest_small_bm:
		case mul_char_one_dest_large_bm:
		case mul_char_two_dest_fullfill:
		case mul_loop_one_dest_fullfill:
		case mul_char_2o3_dest_small_bm:
		case mul_char_2o3_dest_large_bm:
		case mul_char_mul_dest_small_bm:
		case mul_char_mul_dest_large_bm:
		case mul_loop_1o2_dest_small_bm:
		case mul_loop_1o2_dest_large_bm:
		case mul_char_mul_dest_huge4_bm:
		case mul_loop_mul_dest_huge4_bm: // nUniq doesn't include loop target
//		case mul_loop_non_dest_fullfill:
//		case mul_loop_non_dest_small_bm:
//		case mul_loop_non_dest_large_bm:
		case mul_dest_index_mapped:
		case mul_loop_index_mapped:
		case mul_dest_index_mapped_i8:
		case mul_loop_index_mapped_i8:
		case mul_dest_index_mapped_i16:
		case mul_loop_index_mapped_i16:
		case direct_mapped_i8:
			if (u->term_bit) {
				auto pi = reinterpret_cast<const s32*>(u);
				if (pi[slots] < 0)
					slots += 1 + (-pi[slots]); // matchid_cnt
				else
					slots += 1;
			}
			break;
		}
		BOOST_STATIC_ASSERT(sizeof(*u) == 4);
		return reinterpret_cast<const byte_t*>(u + slots);
	}

	state_id_t get_root(size_t idx) const {
		assert(idx < m_roots.size());
		return m_roots[idx];
	}

	size_t num_roots() const {
		assert(m_roots.size() >= 1);
		return m_roots.size();
	}

	template<class Func>
	void for_each_dest_rev(state_id_t state, Func func) const {
		size_t dests[sigma];
		size_t cnt = get_all_dest(state, dests);
		for (size_t i = cnt; i > 0; ) func(dests[--i]);
	}
	template<class Func>
	void for_each_dest(state_id_t state, Func func) const {
		for_each_move(state, [&](size_t t, auchar_t){func(t);});
	}

	template<class Func>
	void for_each_move(state_id_t state, Func func) const {
		const State* u = states.data() + state;
		switch (OpCode(u->op)) {
		default:
			assert(0);
			THROW_STD(logic_error, "invalid OpCode = %d", u->op);
		case leaf_final_state:
			// may be a dead state, initial_state may also be dead
			// assert(u->term_bit || initial_state == state);
			break;
		case all_char_all_jump:
		  {
			state_id_t target = state + 1;
			for (size_t ch = 0; ch < 256; ++ch) func(target, ch);
			break;
		  }
		case one_char_one_diff:
			func(state + u->diff_val, u->ch);
			break;
		case one_char_one_dest: {
			auto p = (const Inst_one_char_one_dest*)(u);
			func(p->target, p->ch);
			break; }
		case all_diff_one_loop: {
			unsigned other = state + u->diff_val;
			unsigned chx = u->ch;
			unsigned ch = 0;
			for (; ch < chx; ++ch) func(other, ch);
			func(state, ch++);
			for (; ch < 256; ++ch) func(other, ch);
			break; }
		case all_loop_one_diff: {
			unsigned other = state + u->diff_val;
			unsigned chx = u->ch;
			unsigned ch = 0;
			for (; ch < chx; ++ch) func(state, ch);
			func(other, ch++);
			for (; ch < 256; ++ch) func(state, ch);
			break; }
		case mul_char_one_diff_fullfill: {
			auto p = (const Inst_mul_char_one_diff_fullfill*)(u);
			unsigned maxch = p->maxch;
			unsigned target = intptr_t(state) + p->diff_val;
			for (unsigned ch = p->minch; ch <= maxch; ++ch) {
				func(target, ch);
			}
			break; }
		case mul_char_one_dest_fullfill: {
			auto p = (const Inst_mul_char_one_dest_fullfill*)(u);
			unsigned maxch = p->maxch;
			unsigned target = p->target;
			for (unsigned ch = p->minch; ch <= maxch; ++ch) {
				func(target, ch);
			}
			break; }
		case neg_char_one_diff_fullfill:
		  {
			auto p = (const Inst_neg_char_one_diff_fullfill*)(u);
			unsigned minch = p->minch;
			uint32_t target = intptr_t(state) + p->diff_val;
			for (unsigned ch = 0; ch < minch; ++ch) {
				func(target, ch);
			}
			for (unsigned ch = p->maxch+1; ch < 256; ++ch) {
				func(target, ch);
			}
			break;
		  }
		case neg_char_one_dest_fullfill:
		  {
			auto p = (const Inst_neg_char_one_dest_fullfill*)(u);
			unsigned minch = p->minch;
			uint32_t target = p->target;
			for (unsigned ch = 0; ch < minch; ++ch) {
				func(target, ch);
			}
			for (unsigned ch = p->maxch+1; ch < 256; ++ch) {
				func(target, ch);
			}
			break;
		  }
		case mul_char_one_dest_small_bm: {
			auto p = (const Inst_mul_char_one_dest_small_bm*)(u);
			u32 bm = p->bits1;
			u32 ch = p->minch;
			u32 target = p->target;
			while (bm) {
				int ctz = fast_ctz(bm);
				ch += ctz;
				bm >>= ctz;
				bm >>= 1;
				func(target, ch++);
			}
			break; }
		case mul_char_one_dest_large_bm: {
			auto p = (const Inst_mul_char_one_dest_large_bm*)(u);
			unsigned ch0 = p->minch;
			unsigned nbm = p->nbm + 1;
			uint32_t target = p->tarbits[nbm];
			{
				unsigned ch = ch0;
				uint32_t bm = p->bits1;
				while (bm) {
					int ctz = fast_ctz(bm);
					ch += ctz;
					bm >>= ctz;
					bm >>= 1;
					func(target, ch++);
				}
				ch0 += 13;
			}
			for(unsigned i = 0; i < nbm; ++i) {
				unsigned ch = ch0;
				uint32_t bm = p->tarbits[i];
				while (bm) {
					int ctz = fast_ctz(bm);
					ch += ctz;
					bm >>= ctz;
					bm >>= 1;
					func(target, ch++);
				}
				ch0 += 32;
			}
			break; }
		case mul_char_two_dest_fullfill:
		  {
			auto p = (const Inst_mul_char_two_dest_fullfill*)(u);
			size_t nbm = p->calc_nbm();
			unsigned minch = p->minch;
			unsigned maxch = p->maxch;
			uint32_t t0 = p->tarbits[nbm+0];
			uint32_t t1 = p->tarbits[nbm+1];
			for(unsigned ch = minch; ch <= maxch; ++ch) {
				intptr_t bi = ch - minch + 32 - 8;
				uint32_t bm = p->tarbits[bi/32 - 1];
				func(bm & (one << bi%32) ? t1 : t0, ch);
			}
			break;
		  }
		case mul_loop_one_dest_fullfill:
		  {
			auto p = (const Inst_mul_loop_one_dest_fullfill*)(u);
			size_t nbm = p->calc_nbm();
			unsigned minch = p->minch;
			unsigned maxch = p->maxch;
			uint32_t target = p->tarbits[nbm+0];
			for(unsigned ch = minch; ch <= maxch; ++ch) {
				intptr_t bi = ch - minch + 32 - 8;
				uint32_t bm = p->tarbits[bi/32 - 1];
				func(bm & (one << bi%32) ? target : state, ch);
			}
			break;
		  }
		case mul_char_2o3_dest_small_bm: {
			auto p = (const Inst_mul_char_2o3_dest_small_bm*)(u);
			unsigned bm = p->bits1;
			unsigned ch = p->minch;
			while (bm) {
				int ctz = fast_ctz(bm) & ~1;
				ch += ctz/2;
				bm >>= ctz;
				func(p->targets[(bm&3)-1], ch++);
				bm >>= 2;
			}
			break; }
		case mul_char_2o3_dest_large_bm: {
			auto p = (const Inst_mul_char_2o3_dest_large_bm*)(u);
			unsigned ch0 = p->minch;
			unsigned nbm = p->nbm + 1;
			auto targets = &p->tarbits[nbm];
			{
				unsigned ch = ch0;
				uint32_t bm = p->bits1;
				while (bm) {
					int ctz = fast_ctz(bm) & ~1;
					ch += ctz/2;
					bm >>= ctz;
					func(targets[(bm&3)-1], ch++);
					bm >>= 2;
				}
				ch0 += 12/2;
			}
			for(unsigned i = 0; i < nbm; ++i) {
				unsigned ch = ch0;
				uint32_t bm = p->tarbits[i];
				while (bm) {
					int ctz = fast_ctz(bm) & ~1;
					ch += ctz/2;
					bm >>= ctz;
					func(targets[(bm&3)-1], ch++);
					bm >>= 2;
				}
				ch0 += 16;
			}
			break; }
		case mul_char_mul_dest_small_bm: {
			auto p = (const Inst_mul_char_mul_dest_small_bm*)(u);
			unsigned bm = p->bits1;
			unsigned ch = p->minch;
			unsigned t1 = p->target1;
			unsigned t2 = p->target2;
			auto ts = p->targets;
			while (bm) {
				int ctz = fast_ctz(bm) & ~1;
				ch += ctz/2;
				bm >>= ctz;
				switch (bm&3) {
					case 0: assert(0); break;
					case 1: func(t1, ch); break;
					case 2: func(t2, ch); break;
					case 3:	func(*ts++, ch); break;
				}
				ch++;
				bm >>= 2;
			}
			break; }
		case mul_char_mul_dest_large_bm: {
			auto p = (const Inst_mul_char_mul_dest_large_bm*)(u);
			unsigned ch0 = p->minch;
			unsigned nbm = ((p->maxch - ch0 + 1) * 2 + 63) / 64 * 2;
			unsigned t1 = p->target1;
			unsigned t2 = p->target2;
			auto ts = p->ubits.bm32 + nbm;
			for(unsigned i = 0; i < nbm; ++i) {
				unsigned ch = ch0;
				uint32_t bm = p->ubits.bm32[i];
				while (bm) {
					int ctz = fast_ctz(bm) & ~1;
					ch += ctz/2;
					bm >>= ctz;
					switch (bm&3) {
						case 0: assert(0); break;
						case 1: func(t1, ch); break;
						case 2: func(t2, ch); break;
						case 3:	func(*ts++, ch); break;
					}
					ch++;
					bm >>= 2;
				}
				ch0 += 16;
			}
			break; }
		case mul_loop_1o2_dest_small_bm: {
			auto p = (const Inst_mul_loop_1o2_dest_small_bm*)(u);
			unsigned bm = p->bits1;
			unsigned ch = p->minch;
			while (bm) {
				int ctz = fast_ctz(bm) & ~1;
				bm >>= ctz;
				ch += ctz/2;
				switch (bm&3) {
				case 0: assert(0);               break;
				case 1: func(state        , ch); break;
				case 2: func(p->targets[0], ch); break;
				case 3: func(p->targets[1], ch); break;
				}
				ch++;
				bm >>= 2;
			}
			break; }
		case mul_loop_1o2_dest_large_bm: {
			auto p = (const Inst_mul_loop_1o2_dest_large_bm*)(u);
			unsigned ch0 = p->minch;
			unsigned nbm = p->nbm+1;
			{
				unsigned bm = p->bits1;
				unsigned ch = ch0;
				while (bm) {
					int ctz = fast_ctz(bm) & ~1;
					bm >>= ctz;
					ch += ctz/2;
					switch (bm&3) {
					case 0: assert(0);                   break;
					case 1: func(state            , ch); break;
					case 2: func(p->tarbits[nbm+0], ch); break;
					case 3: func(p->tarbits[nbm+1], ch); break;
					}
					ch++;
					bm >>= 2;
				}
				ch0 += 12/2;
			}
			for(unsigned i = 0; i < nbm; ++i) {
				unsigned bm = p->tarbits[i];
				unsigned ch = ch0;
				while (bm) {
					int ctz = fast_ctz(bm) & ~1;
					bm >>= ctz;
					ch += ctz/2;
					switch (bm&3) {
					case 0: assert(0);                   break;
					case 1: func(state            , ch); break;
					case 2: func(p->tarbits[nbm+0], ch); break;
					case 3: func(p->tarbits[nbm+1], ch); break;
					}
					ch++;
					bm >>= 2;
				}
				ch0 += 16;
			}
			break; }
		case mul_char_mul_dest_huge4_bm:
		  {
			auto p = (const Inst_mul_char_mul_dest_huge4_bm*)(u);
			u32 ch0 = p->minch;
			size_t nbm = ((p->maxch - p->minch + 1) * 4 + 31) / 32;
			auto targets = p->tarbits + nbm;
			for (size_t i = 0; i < nbm; ++i) {
				u32 bm = p->tarbits[i];
				u32 ch = ch0;
				while (bm) {
					int ctz = fast_ctz32(bm) & ~3;
					bm >>= ctz;
					ch += ctz/4;
					size_t idx = bm & 15;
					assert(idx > 0);
					assert(idx < size_t(p->nUniq+1));
					func(targets[idx - 1], ch++);
					bm >>= 4;
				}
				ch0 += 8;
			}
			break;
		  }
		case mul_loop_mul_dest_huge4_bm: // nUniq doesn't include loop target
		  {
			auto p = (const Inst_mul_loop_mul_dest_huge4_bm*)(u);
			u32 ch0 = p->minch;
			size_t nbm = ((p->maxch - p->minch + 1) * 4 + 31) / 32;
			auto targets = p->tarbits + nbm;
			for (size_t i = 0; i < nbm; ++i) {
				u32 bm = p->tarbits[i];
				u32 ch = ch0;
				while (bm) {
					int ctz = fast_ctz32(bm) & ~3;
					bm >>= ctz;
					ch += ctz/4;
					size_t idx = bm & 15;
					assert(idx > 0);
					assert(idx < size_t(p->nUniq+2));
					if (1 == idx)
						func(state, ch);
					else
						func(targets[idx-2], ch);
					bm >>= 4;
					ch++;
				}
				ch0 += 8;
			}
			break;
		  }
#if 0
		case mul_loop_non_dest_fullfill: {
			auto p = (const Inst_mul_loop_non_dest_fullfill*)(u);
			unsigned maxch = p->maxch;
			for (unsigned ch = p->minch; ch <= maxch; ++ch) {
				func(state, ch);
			}
			break; }
		case mul_loop_non_dest_small_bm: {
			auto p = (const Inst_mul_loop_non_dest_small_bm*)(u);
			unsigned bm = p->bits1;
			unsigned ch = p->minch;
			while (bm) {
				int ctz = fast_ctz(bm);
				ch += ctz;
				bm >>= ctz;
				bm >>= 1;
				func(state, ch++);
			}
			break; }
		case mul_loop_non_dest_large_bm: {
			auto p = (const Inst_mul_loop_non_dest_large_bm*)(u);
			unsigned ch0 = p->minch;
			unsigned nbm = p->nbm + 1;
			{
				unsigned bm = p->bits1;
				unsigned ch = ch0;
				while (bm) {
					int ctz = fast_ctz(bm);
					ch += ctz;
					bm >>= ctz;
					bm >>= 1;
					func(state, ch++);
				}
				ch0 += 13;
			}
			for(unsigned i = 0; i < nbm; ++i) {
				unsigned bm = p->bitvec[i];
				unsigned ch = ch0;
				while (bm) {
					int ctz = fast_ctz(bm);
					ch += ctz;
					bm >>= ctz;
					bm >>= 1;
					func(state, ch++);
				}
				ch0 += 32;
			}
			break; }
#endif
		case all_dest_one_loop: {
			auto p = reinterpret_cast<const Inst_one_char_one_dest*>(u);
			unsigned ch = 0;
			unsigned chx = p->ch;
			unsigned other = p->target;
			for (; ch < chx; ++ch) func(other, ch);
			func(state, ch++);
			for (; ch < 256; ++ch) func(other, ch);
			break; };
		case all_loop_one_dest: { /// "a.*bcd"
			auto p = reinterpret_cast<const Inst_one_char_one_dest*>(u);
			unsigned ch = 0;
			unsigned chx = p->ch;
			for (; ch < chx; ++ch) func(state, ch);
			func(p->target, ch++);
			for (; ch < 256; ++ch) func(state, ch);
			break; }
		case mul_dest_index_mapped:
		  {
			auto p = ((const Inst_mul_dest_index_mapped*)(u));
			auto& me = m_index_map[p->map_id];
			for(size_t ch = 0; ch < 256; ++ch) {
				size_t ch_id = me.index[ch];
				if (255 != ch_id) {
					assert(ch_id < me.n_uniq);
					func(p->targets[ch_id], ch);
				}
			}
			break;
		  }
		case mul_loop_index_mapped:
		  {
			auto p = ((const Inst_mul_dest_index_mapped*)(u));
			auto& me = m_index_map[p->map_id];
			for(size_t ch = 0; ch < 256; ++ch) {
				size_t ch_id = me.index[ch];
				if (255 != ch_id) {
					assert(ch_id < me.n_uniq);
					if (ch_id)
						func(p->targets[ch_id-1], ch);
					else
						func(state, ch); // loop
				}
			}
			break;
		  }
		case mul_dest_index_mapped_i8:
		  {
			auto p = ((const Inst_mul_dest_index_mapped_i8*)(u));
			auto& me = m_index_map[p->map_id];
			for(size_t ch = 0; ch < 256; ++ch) {
				size_t ch_id = me.index[ch];
				if (255 != ch_id) {
					assert(ch_id < me.n_uniq);
					func(intptr_t(state) + p->diffs_i8[ch_id], ch);
				}
			}
			break;
		  }
		case mul_loop_index_mapped_i8:
		  {
			auto p = ((const Inst_mul_dest_index_mapped_i8*)(u));
			auto& me = m_index_map[p->map_id];
			for(size_t ch = 0; ch < 256; ++ch) {
				size_t ch_id = me.index[ch];
				if (255 != ch_id) {
					assert(ch_id < me.n_uniq);
					if (ch_id)
						func(intptr_t(state) + p->diffs_i8[ch_id-1], ch);
					else
						func(state, ch); // loop
				}
			}
			break;
		  }
		case mul_dest_index_mapped_i16:
		  {
			auto p = ((const Inst_mul_dest_index_mapped_i16*)(u));
			auto& me = m_index_map[p->map_id];
			for(size_t ch = 0; ch < 256; ++ch) {
				size_t ch_id = me.index[ch];
				if (255 != ch_id) {
					assert(ch_id < me.n_uniq);
					func(intptr_t(state) + p->diffs_i16[ch_id], ch);
				}
			}
			break;
		  }
		case mul_loop_index_mapped_i16:
		  {
			auto p = ((const Inst_mul_dest_index_mapped_i16*)(u));
			auto& me = m_index_map[p->map_id];
			for(size_t ch = 0; ch < 256; ++ch) {
				size_t ch_id = me.index[ch];
				if (255 != ch_id) {
					assert(ch_id < me.n_uniq);
					if (ch_id)
						func(intptr_t(state) + p->diffs_i16[ch_id-1], ch);
					else
						func(state, ch); // loop
				}
			}
			break;
		  }
		case direct_mapped_i8:
		  {
			auto p = ((const BaseInst_index_mapped*)(u));
			auto& me = m_index_map[p->map_id];
			for(size_t ch = 0; ch < 256; ++ch) {
				intptr_t diff = int8_t(me.index[ch]);
				if (-128 != diff)
					func(intptr_t(state) + diff, ch);
			}
			break;
		  }
		}
	}

	inline
	size_t state_move(size_t state, auchar_t ch) const {
		assert(ch < sigma);
		assert(state < states.size());
		const State* u = states.data() + state;
		switch (OpCode(u->op)) {
		default:
			assert(0);
			THROW_STD(logic_error, "invalid OpCode = %d", u->op);
		case leaf_final_state:
			// may be a dead state, initial_state may also be dead
			// assert(u->term_bit || initial_state == state);
			return nil_state;
		case all_char_all_jump:
			return state + 1;
		case one_char_one_diff:
			if (ch == u->ch)
				return intptr_t(state) + u->diff_val;
			break;
		case one_char_one_dest: {
			auto p = (const Inst_one_char_one_dest*)(u);
			if (ch == p->ch)
				return p->target;
			break; }
		case all_diff_one_loop:
			if (ch == u->ch)
				 return state;
			else return intptr_t(state) + u->diff_val;
		case all_loop_one_diff:
			if (ch != u->ch)
				 return state;
			else return intptr_t(state) + u->diff_val;
		case mul_char_one_diff_fullfill: {
			auto p = (const Inst_mul_char_one_diff_fullfill*)(u);
			if (ch >= p->minch && ch <= p->maxch)
				return intptr_t(state) + p->diff_val;
			break; }
		case mul_char_one_dest_fullfill: {
			auto p = (const Inst_mul_char_one_dest_fullfill*)(u);
			if (ch >= p->minch && ch <= p->maxch)
				return p->target;
			break; }
		case neg_char_one_diff_fullfill:
		  {
			auto p = (const Inst_neg_char_one_diff_fullfill*)(u);
			if (!(ch >= p->minch && ch <= p->maxch))
				return intptr_t(state) + p->diff_val;
			break;
		  }
		case neg_char_one_dest_fullfill:
		  {
			auto p = (const Inst_neg_char_one_dest_fullfill*)(u);
			if (!(ch >= p->minch && ch <= p->maxch))
				return p->target;
			break;
		  }
		case mul_char_one_dest_small_bm: {
			auto p = (const Inst_mul_char_one_dest_small_bm*)(u);
			unsigned minch = p->minch;
			if (ch >= minch && ch < minch + 16)
				if ((p->bits1 >> (ch - minch)) & 1)
					return p->target;
			break; }
		case mul_char_one_dest_large_bm: {
			auto p = (const Inst_mul_char_one_dest_large_bm*)(u);
			unsigned minch = p->minch;
			unsigned maxch = p->minch + p->nbm*32 + 32 + 13;
			if (ch >= minch && ch < maxch) {
				intptr_t idx = ch - minch + (32-13);
				if ((p->tarbits[idx/32 - 1] >> idx%32) & 1)
					return p->tarbits[p->nbm+1]; // the target
			}
			break; }
		case mul_char_two_dest_fullfill:
		  {
			auto p = (const Inst_mul_char_two_dest_fullfill*)(u);
			unsigned minch = p->minch;
			unsigned maxch = p->maxch;
			if (ch >= minch && ch <= maxch) {
				size_t nbm = (maxch-minch+1+31-8)/32;
				intptr_t idx = ch - minch + 32 - 8;
				return p->tarbits[nbm + ((p->tarbits[idx/32-1]>>idx%32)&1)];
			}
			break;
		  }
		case mul_loop_one_dest_fullfill:
		  {
			auto p = (const Inst_mul_loop_one_dest_fullfill*)(u);
			unsigned minch = p->minch;
			unsigned maxch = p->maxch;
			if (ch >= minch && ch <= maxch) {
				intptr_t idx = ch - minch + 32 - 8;
				if ((p->tarbits[idx/32-1]>>idx%32)&1)
					 return p->tarbits[(maxch-minch+1+31-8)/32];
				else return state; // loop on 0==bit
			}
			break;
		  }
		case mul_char_2o3_dest_small_bm: {
			auto p = (const Inst_mul_char_2o3_dest_small_bm*)(u);
			unsigned minch = p->minch;
			if (ch >= minch && ch < minch + 8) {
				unsigned bval = (p->bits1 >> (ch - minch)*2) & 3;
				if (bval)
					return p->targets[bval-1];
			}
			break; }
		case mul_char_2o3_dest_large_bm: {
			auto p = (const Inst_mul_char_2o3_dest_large_bm*)(u);
			unsigned nbm32 = p->nbm;
			unsigned minch = p->minch;
		//	unsigned maxch = p->minch + (nbm32*32 + 32 + 12)/2;
			unsigned maxch = p->minch +  nbm32*16 + 16 +  6;
			if (ch >= minch && ch < maxch) {
				intptr_t idx = (ch - minch)*2 + (32-12);
				unsigned bval = (p->tarbits[idx/32-1] >> idx%32) & 3;
				if (bval)
				//	return p->tarbits[(nbm32+1) + (bval-1)];
					return p->tarbits[nbm32 + bval]; // may be faster
			}
			break; }
		case mul_char_mul_dest_small_bm: {
			auto p = (const Inst_mul_char_mul_dest_small_bm*)(u);
			unsigned minch = p->minch;
			if (ch >= minch && ch < minch + 8) {
				unsigned bpos = (ch - minch) * 2;
				unsigned bm = p->bits1;
				switch ((bm >> bpos) & 3) {
				case 0: return nil_state;
				case 1: return p->target1;
				case 2: return p->target2;
				case 3:
					bm = ((bm >> 1) & bm) & BitMask32;
					return p->targets[fast_popcount_trail(bm, bpos)];
				}
			}
			break; }
		case mul_char_mul_dest_large_bm: {
			auto p = (const Inst_mul_char_mul_dest_large_bm*)(u);
			unsigned minch = p->minch;
			unsigned maxch = p->maxch;
			if (ch >= minch && ch <= maxch) {
				intptr_t idx = (ch - minch) * 2;
				uint64_t b64 = p->ubits.bm64[idx / 64];
				switch ((b64 >> idx % 64) & 3) {
				case 0: return nil_state;
				case 1: return p->target1;
				case 2: return p->target2;
				case 3:
				  {
					intptr_t nbm64 = ((maxch - minch + 1)*2 + 63) / 64;
					intptr_t rank = p->rank[idx / 64];
					const u32* targets = p->ubits.bm32 + nbm64*2;
					b64 = b64 & (b64 >> 1) & BitMask64;
					u32 t = targets[rank + fast_popcount_trail(b64, idx%64)];
					assert(t < states.size());
					return t;
				  }
				}
			}
			break; }
		case mul_loop_1o2_dest_small_bm: {
			auto p = (const Inst_mul_loop_1o2_dest_small_bm*)(u);
			unsigned minch = p->minch;
			if (ch >= minch && ch < minch + 8) {
				u32 bval = (p->bits1 >> (ch - minch) * 2) & 3;
				u32 t[4] = {nil_state, u32(state), p->targets[0], p->targets[1]};
				return t[bval];
			}
			break; }
		case mul_loop_1o2_dest_large_bm: {
			auto p = (const Inst_mul_loop_1o2_dest_large_bm*)(u);
			unsigned nbm32 = p->nbm;
			unsigned minch = p->minch;
		//	unsigned maxch = p->minch + (nbm32*32 + 32 + 12)/2;
			unsigned maxch = p->minch +  nbm32*16 + 16 +  6;
			if (ch >= minch && ch < maxch) {
				intptr_t idx = (ch - minch)*2 + (32-12);
				unsigned bval = (p->tarbits[idx/32-1] >> idx%32) & 3;
				switch (bval) {
				case 0: return nil_state;
				case 1: return state;
				case 2: case 3:
					return p->tarbits[nbm32 + bval - 1];
				}
			}
			break; }
		case mul_char_mul_dest_huge4_bm:
		  {
			auto p = (const Inst_mul_char_mul_dest_huge4_bm*)(u);
			u32 minch = p->minch;
			u32 maxch = p->maxch;
			if (ch >= minch && ch <= maxch) {
				size_t nbm = ((maxch - minch + 1) * 4 + 31) / 32;
				size_t idx = (ch - minch) * 4;
				size_t jdx = (p->tarbits[idx/32] >> idx%32) & 15;
				assert(jdx < size_t(p->nUniq + 1));
				if (jdx)
					return p->tarbits[nbm + jdx - 1];
			}
			break;
		  }
		case mul_loop_mul_dest_huge4_bm: // nUniq doesn't include loop target
		  {
			auto p = (const Inst_mul_loop_mul_dest_huge4_bm*)(u);
			u32 minch = p->minch;
			u32 maxch = p->maxch;
			if (ch >= minch && ch <= maxch) {
				size_t nbm = ((maxch - minch + 1) * 4 + 31) / 32;
				size_t idx = (ch - minch) * 4;
				size_t jdx = (p->tarbits[idx/32] >> idx%32) & 15;
				assert(jdx < size_t(p->nUniq + 2));
				if (jdx) {
					if (1 == jdx)
						return state;
					return p->tarbits[nbm + jdx - 2];
				}
			}
			break;
		  }
#if 0
		case mul_loop_non_dest_fullfill:
		  {
			auto p = (const Inst_mul_loop_non_dest_fullfill*)(u);
			if (ch >= p->minch && ch <= p->maxch)
				return state;
			break;
		  }
		case mul_loop_non_dest_small_bm: {
			auto p = (const Inst_mul_loop_non_dest_small_bm*)(u);
			unsigned minch = p->minch;
			if (ch >= minch && ch < minch + 16)
				if ((p->bits1 >> (ch - minch)) & 1)
					return state;
			break; }
		case mul_loop_non_dest_large_bm: {
			auto p = (const Inst_mul_loop_non_dest_large_bm*)(u);
			unsigned minch = p->minch;
			unsigned maxch = p->minch + p->nbm*32 + 32 + 13;
			if (ch >= minch && ch < maxch) {
				intptr_t idx = ch - minch + (32-13);
				if ((p->bitvec[idx/32 - 1] >> idx%32) & 1)
					return state;
			}
			break; }
#endif
		case all_dest_one_loop: {
			auto p = (const Inst_one_char_one_dest*)(u);
			if (ch == p->ch)
				return state; // loop
			else
				return p->target;
			break; }
		case all_loop_one_dest: { /// "a.*bcd"
			auto p = (const Inst_one_char_one_dest*)(u);
			if (ch != p->ch)
				return state; // loop
			else
				return p->target;
			break; }
		case mul_dest_index_mapped:
		  {
			auto p = ((const Inst_mul_dest_index_mapped*)(u));
			auto& me = m_index_map[p->map_id];
			size_t ch_id = me.index[ch];
			if (255 != ch_id) {
				assert(ch_id < me.n_uniq);
				return p->targets[ch_id];
			}
			break;
		  }
		case mul_loop_index_mapped:
		  {
			auto p = ((const Inst_mul_dest_index_mapped*)(u));
			auto& me = m_index_map[p->map_id];
			size_t ch_id = me.index[ch];
			if (255 != ch_id) {
				assert(ch_id < me.n_uniq);
				if (ch_id)
					return p->targets[ch_id - 1];
				else
					return state;
			}
			break;
		  }
		case mul_dest_index_mapped_i8:
		  {
			auto p = ((const Inst_mul_dest_index_mapped_i8*)(u));
			auto& me = m_index_map[p->map_id];
			size_t ch_id = me.index[ch];
			if (255 != ch_id) {
				assert(ch_id < me.n_uniq);
				return intptr_t(state) + p->diffs_i8[ch_id];
			}
			break;
		  }
		case mul_loop_index_mapped_i8:
		  {
			auto p = ((const Inst_mul_dest_index_mapped_i8*)(u));
			auto& me = m_index_map[p->map_id];
			size_t ch_id = me.index[ch];
			if (255 != ch_id) {
				assert(ch_id < me.n_uniq);
				if (ch_id)
					return intptr_t(state) + p->diffs_i8[ch_id - 1];
				else
					return state;
			}
			break;
		  }
		case mul_dest_index_mapped_i16:
		  {
			auto p = ((const Inst_mul_dest_index_mapped_i16*)(u));
			auto& me = m_index_map[p->map_id];
			size_t ch_id = me.index[ch];
			if (255 != ch_id) {
				assert(ch_id < me.n_uniq);
				return intptr_t(state) + p->diffs_i16[ch_id];
			}
			break;
		  }
		case mul_loop_index_mapped_i16:
		  {
			auto p = ((const Inst_mul_dest_index_mapped_i16*)(u));
			auto& me = m_index_map[p->map_id];
			size_t ch_id = me.index[ch];
			if (255 != ch_id) {
				assert(ch_id < me.n_uniq);
				if (ch_id)
					return intptr_t(state) + p->diffs_i16[ch_id - 1];
				else
					return state;
			}
			break;
		  }
		case direct_mapped_i8:
		  {
			auto p = ((const BaseInst_index_mapped*)(u));
			auto& me = m_index_map[p->map_id];
			intptr_t diff = int8_t(me.index[ch]);
			if (-128 != diff)
				return intptr_t(state) + diff;
		  }
		}
		return nil_state;
	}

	template<class TR, class InIter>
	friend size_t
	dfa_loop_state_move
	(const VirtualMachineDFA& dfa, size_t state, InIter& beg, InIter end, TR tr)
	{ return dfa.loop_state_move<TR>(state, beg, end, tr); }

	template<class TR, class InIter>
	size_t
	loop_state_move(size_t state, InIter& beg, InIter end, TR tr)
	const {
	//	printf("%s\n", BOOST_CURRENT_FUNCTION);
	//	assert(beg < end); // now RandomIterator
		assert(state < states.size());
		const State* u = states.data() + state;
		assert(!u->pzip_bit);
		switch (u->op) {
		default:
			return dfa_loop_state_move_aux<TR>(*this, state, beg, end, tr);
		case all_diff_one_loop: {
			auto p = (const Inst_one_char_one_diff*)(u);
			size_t target = state + intptr_t(p->diff_val);
			byte_t ch = p->ch;
			for (InIter pos = beg; pos != end; ++pos) {
				if ((byte_t)tr(byte_t(*pos)) != ch) {
					beg = ++pos;
					return target;
				}
			}
			break; }
		case all_loop_one_diff: {
			auto p = (const Inst_one_char_one_diff*)(u);
			size_t target = state + intptr_t(p->diff_val);
			byte_t ch = p->ch;
			for (InIter pos = beg; pos != end; ++pos) {
				if ((byte_t)tr(byte_t(*pos)) == ch) {
					beg = ++pos;
					return target;
				}
			}
			break; }
		case mul_char_one_diff_fullfill:
		  {
			auto p = (const Inst_mul_char_one_diff_fullfill*)(u);
			unsigned minch = p->minch;
			unsigned maxch = p->maxch;
			size_t  target = state + intptr_t(p->diff_val);
			if (target == state) {
				for (InIter pos = beg; pos != end; ) {
					unsigned ch = (byte_t)tr(byte_t(*pos));
					++pos;
					if (!(ch >= minch && ch <= maxch))
						return nil_state;
				}
				break;
			}
		   	else {
				unsigned ch = (byte_t)tr(byte_t(*beg));
				++beg;
				return ch >= minch && ch <= maxch ? target : nil_state;
			}
		  }
		case neg_char_one_diff_fullfill:
		  {
			auto p = (const Inst_mul_char_one_diff_fullfill*)(u);
			unsigned minch = p->minch;
			unsigned maxch = p->maxch;
			size_t   other = state + intptr_t(p->diff_val);
			if (other == state) {
				for (InIter pos = beg; pos != end; ) {
					unsigned ch = (byte_t)tr(byte_t(*pos));
					++pos;
					if (ch >= minch && ch <= maxch) {
						beg = pos;
						return nil_state;
					}
				}
				break;
			}
			else {
				unsigned ch = (byte_t)tr(byte_t(*beg));
				++beg;
				return ch >= minch && ch <= maxch ? nil_state : other;
			}
		  }
		case mul_loop_one_dest_fullfill:
		  {
			auto p = (const Inst_mul_loop_one_dest_fullfill*)(u);
			unsigned minch = p->minch;
			unsigned maxch = p->maxch;
			for (InIter pos = beg; pos != end; ) {
				unsigned ch = (byte_t)tr(byte_t(*pos));
				++pos;
				if (ch >= minch && ch <= maxch) {
					intptr_t bi = ch - minch + 32 - 8;
					uint32_t bm = p->tarbits[bi/32 - 1];
					if (bm & (one << bi%32)) {
						beg = pos;
						return p->tarbits[(maxch-minch+1+31-8)/32];
					}
				} else {
					beg = pos;
					return nil_state;
				}
			}
			break;
		  }
		case mul_loop_1o2_dest_small_bm: {
			auto p = (const Inst_mul_loop_1o2_dest_small_bm*)(u);
			unsigned minch = p->minch;
			unsigned maxch = minch + 8;
			for (InIter pos = beg; pos != end; ++pos) {
				unsigned ch = (byte_t)tr(byte_t(*pos));
				if (ch >= minch && ch < maxch) { // [min, max)
					size_t bidx = 2 * (ch - minch);
					size_t bval = (p->bits1 >> bidx) & 3;
					if (1 == bval)
						continue; // loop, 'continue' makes it faster
					switch (bval) {
						case 0: beg = ++pos; return nil_state;
					//	case 1: break; // loop
						case 2: beg = ++pos; return p->targets[0];
						case 3: beg = ++pos; return p->targets[1];
					}
				} else {
					beg = ++pos;
					return nil_state;
				}
			}
			break; }
		case mul_loop_1o2_dest_large_bm: {
			auto p = (const Inst_mul_loop_1o2_dest_large_bm*)(u);
			unsigned minch = p->minch;
			unsigned maxch = std::min<u32>(256, minch + (p->nbm*32+32+12)/2);
			auto targets = &p->tarbits[p->nbm + 1];
			for (InIter pos = beg; pos != end; ) {
				unsigned ch = (byte_t)tr(byte_t(*pos));
				++pos;
				if (ch >= minch && ch < maxch) { // [min, max)
					intptr_t bidx = 2 * (ch - minch) + (32-12);
					intptr_t bval = (p->tarbits[bidx/32-1] >> bidx%32) & 3;
					if (1 == bval)
						continue; // loop, 'continue' makes it faster
					switch (bval) {
						case 0: beg = pos; return nil_state;
					//	case 1: break; // loop
						case 2: beg = pos; return targets[0];
						case 3: beg = pos; return targets[1];
					}
				} else {
					beg = pos;
					return nil_state;
				}
			}
			break; }
		case mul_loop_mul_dest_huge4_bm: // nUniq doesn't include loop target
		  {
			auto p = (const Inst_mul_loop_mul_dest_huge4_bm*)(u);
			byte_t minch = p->minch;
			byte_t maxch = p->maxch;
			for (InIter pos = beg; pos != end; ) {
				byte_t ch = (byte_t)tr(byte_t(*pos));
				++pos;
				if (ch >= minch && ch <= maxch) { // [min, max]
					size_t idx = (ch - minch) * 4;
					size_t jdx = (p->tarbits[idx/32] >> idx%32) & 15;
					assert(jdx < size_t(p->nUniq) + 2);
					if (jdx) {
						if (1 == jdx)
							continue;
						else {
							size_t nbm = ((maxch-minch+1)*4 + 31) / 32;
							beg = pos;
							return p->tarbits[nbm + jdx - 2];
						}
					} else {
						beg = pos;
						return nil_state;
					}
				} else {
					beg = pos;
					return nil_state;
				}
			}
			break;
		  }
#if 0
		case mul_loop_non_dest_fullfill: {
			auto p = (const Inst_mul_loop_non_dest_fullfill*)(u);
			unsigned minch = p->minch;
			unsigned maxch = p->maxch;
			for (InIter pos = beg; pos != end; ++pos) {
				unsigned ch = (byte_t)tr(byte_t(*pos));
				if (!(ch >= minch && ch <= maxch)) { // [min, max]
					beg = ++pos;
					return nil_state;
				}
			}
			break; }
		case mul_loop_non_dest_small_bm: {
			auto p = (const Inst_mul_loop_non_dest_small_bm*)(u);
			unsigned minch = p->minch;
			unsigned maxch = p->minch + 16;
			unsigned bits1 = p->bits1;
			for (InIter pos = beg; pos != end; ++pos) {
				unsigned ch = (byte_t)tr(byte_t(*pos));
				if (!(ch >= minch && ch < maxch) // [min, max)
					   	|| !((bits1 >> (ch - minch)) & 1)) {
					beg = ++pos;
					return nil_state;
				}
			}
			break; }
		case mul_loop_non_dest_large_bm: {
			auto p = (const Inst_mul_loop_non_dest_large_bm*)(u);
			unsigned minch = p->minch;
			unsigned maxch = p->minch + p->nbm*32 + 32 + 13;
			for (InIter pos = beg; pos != end; ++pos) {
				unsigned ch = (byte_t)tr(byte_t(*pos));
				if (ch >= minch && ch < maxch) { // [min, max)
					intptr_t idx = ch - minch + (32-13);
					if (!((p->bitvec[idx/32 - 1] >> idx%32) & 1))
						goto mul_loop_non_dest_large_bm_fail;
				} else {     mul_loop_non_dest_large_bm_fail:
					beg = ++pos;
					return nil_state;
				}
			}
			break; }
#endif
		case all_dest_one_loop: {
			auto p = reinterpret_cast<const Inst_one_char_one_dest*>(u);
			byte_t ch = p->ch;
			// find_not ch in range[beg, end)
			for (InIter pos = beg; pos != end; ++pos) {
				if ((byte_t)tr(byte_t(*pos)) != ch) { // found non-ch
					beg = ++pos;
					return p->target;
				}
			}
			break; }
		case all_loop_one_dest: { /// "a.*bcd"
			auto p = reinterpret_cast<const Inst_one_char_one_dest*>(u);
			byte_t ch = p->ch;
			// find ch in range[beg, end)
			for (InIter pos = beg; pos != end; ++pos) {
				if ((byte_t)tr(byte_t(*pos)) == ch) { // found ch
					beg = ++pos;
					return p->target;
				}
			}
			break; }
		case all_char_all_jump:
		  {
			auto p = reinterpret_cast<const Inst_all_char_all_jump*>(u);
		//	size_t n_jump = std::min<size_t>(std::distance(beg, end), p->max_jump);
		//	std::advance(beg, n_jump);
		//	now only random_access_iterator
			size_t n_jump = std::min<size_t>(end - beg, p->max_jump);
			beg += n_jump;
			return state + n_jump;
		  }
		case mul_loop_index_mapped:
		  {
			auto p = reinterpret_cast<const Inst_mul_loop_index_mapped*>(u);
			assert(p->map_id < m_index_map.size());
			auto& me = m_index_map[p->map_id];
			for(InIter pos = beg; pos != end; ) {
				size_t ch_id = me.index[(byte_t)tr(byte_t(*pos))];
				++pos;
				if (0 == ch_id) {
					// loop
				}
				else if (255 == ch_id) {
					beg = pos;
					return nil_state;
				}
				else {
					assert(ch_id < me.n_uniq);
					beg = pos;
					return p->targets[ch_id-1];
				}
			}
			break;
		  }
		case mul_loop_index_mapped_i8:
		  {
			auto p = reinterpret_cast<const Inst_mul_loop_index_mapped_i8*>(u);
			assert(p->map_id < m_index_map.size());
			auto& me = m_index_map[p->map_id];
			for(InIter pos = beg; pos != end; ) {
				size_t ch_id = me.index[(byte_t)tr(byte_t(*pos))];
				++pos;
				if (0 == ch_id) {
					// loop
				}
				else if (255 == ch_id) {
					beg = pos;
					return nil_state;
				}
				else {
					assert(ch_id < me.n_uniq);
					beg = pos;
					return intptr_t(state) + p->diffs_i8[ch_id-1];
				}
			}
			break;
		  }
		case mul_loop_index_mapped_i16:
		  {
			auto p = reinterpret_cast<const Inst_mul_loop_index_mapped_i16*>(u);
			assert(p->map_id < m_index_map.size());
			auto& me = m_index_map[p->map_id];
			for(InIter pos = beg; pos != end; ) {
				size_t ch_id = me.index[(byte_t)tr(byte_t(*pos))];
				++pos;
				if (0 == ch_id) {
					// loop
				}
				else if (255 == ch_id) {
					beg = pos;
					return nil_state;
				}
				else {
					assert(ch_id < me.n_uniq);
					beg = pos;
					return intptr_t(state) + p->diffs_i16[ch_id-1];
				}
			}
			break;
		  }
		case direct_mapped_i8:
		  {
			auto p = ((const BaseInst_index_mapped*)(u));
			auto& me = m_index_map[p->map_id];
			for (InIter pos = beg; pos != end;) {
				intptr_t diff = int8_t(me.index[(byte_t)tr(byte_t(*pos))]);
				++pos;
				if (0 == diff) {
					// loop
				}
				else if (-128 == diff) {
					beg = pos;
					return nil_state;
				}
				else {
					beg = pos;
					return intptr_t(state) + diff;
				}
			}
			break;
		  }
		}
		beg = end;
		return state;
	}

	template<class Au>
	void build_from(const Au& au) {
		m_roots.resize_no_init(1);
		m_roots[0] = initial_state;
		const size_t root = initial_state;
		build_from(au, &root, 1, &m_roots[0]);
	}

	template<class Au, class AuID>
	void build_from(const Au& au, const valvec<AuID>& src_roots) {
		m_roots.resize_no_init(src_roots.size());
		build_from(au, src_roots.data(), src_roots.size(), m_roots.data());
	}

	template<class Au, class AuID>
	void build_from(const Au& au
			, const AuID* src_roots, size_t num_roots
			, state_id_t* dst_roots);

	size_t m_atom_dfa_num;
	size_t m_dfa_cluster_num;

protected:
	valvec<state_t> states;
	valvec<state_id_t> m_roots;
	size_t m_gnode_states;
	size_t m_transition_num;
	class Builder;

	struct IndexMapEntry {
		uint32_t refcnt;
		uint16_t n_uniq;
		uint16_t n_char;
		uint08_t index[256];
	};
	valvec<IndexMapEntry> m_index_map;

public:
typedef VirtualMachineDFA MyType;
//#include "ppi/for_each_suffix.hpp"
//#include "ppi/match_key.hpp"
//#include "ppi/match_path.hpp"
//#include "ppi/match_prefix.hpp"
#include "ppi/dfa_reverse.hpp"
#include "ppi/dfa_const_virtuals.hpp"
//#include "ppi/dfa_const_methods_use_walk.hpp"
//#include "ppi/post_order.hpp"
#include "ppi/state_move_fast.hpp"
#include "ppi/flat_dfa_data_io.hpp"
//#include "ppi/flat_dfa_mmap.hpp"
//#include "ppi/for_each_word.hpp"

	void finish_load_mmap(const DFA_MmapHeader* base) override {
		assert(sizeof(State) == base->state_size);
		byte_t* bbase = (byte_t*)base;
		if (base->total_states >= max_state) {
			THROW_STD(out_of_range, "total_states=%lld", (long long)base->total_states);
		}
		states.clear();
		states.risk_set_data((State*)(bbase + base->blocks[0].offset));
		states.risk_set_size(size_t(base->total_states));
		states.risk_set_capacity(size_t(base->total_states));
		assert(base->blocks[1].length > 0);
		assert(base->blocks[1].length % sizeof(state_id_t) == 0);
		size_t root_num = base->blocks[1].length / sizeof(state_id_t);
		m_roots.risk_set_data((state_id_t*)(bbase + base->blocks[1].offset));
		m_roots.risk_set_size(root_num);
		m_roots.risk_set_capacity(root_num);
		size_t index_map_size = base->blocks[2].length / sizeof(IndexMapEntry);
		m_index_map.risk_set_data((IndexMapEntry*)(bbase + base->blocks[2].offset));
		m_index_map.risk_set_size(index_map_size);
		m_index_map.risk_set_capacity(index_map_size);
		m_gnode_states = size_t(base->gnode_states);
		m_zpath_states = size_t(base->zpath_states);
		m_atom_dfa_num = base->atom_dfa_num;
		m_dfa_cluster_num = base->dfa_cluster_num;
		m_total_zpath_len = base->zpath_length;
		m_transition_num = base->transition_num;
		assert(m_dfa_cluster_num + m_atom_dfa_num == root_num);
	}

	long prepare_save_mmap(DFA_MmapHeader* base, const void** dataPtrs)
	const override {
		base->state_size = sizeof(State);
		base->num_blocks = 3;
		base->atom_dfa_num = m_atom_dfa_num;
		base->dfa_cluster_num = m_roots.size() - m_atom_dfa_num;
		base->zpath_length = m_total_zpath_len;
		base->zpath_states = m_zpath_states;
		base->transition_num = m_transition_num;

		base->blocks[0].offset = sizeof(DFA_MmapHeader);
		base->blocks[0].length = sizeof(State)*states.size();
		base->blocks[1].offset = align_to_64(base->blocks[0].endpos());
		base->blocks[1].length = m_roots.size() * sizeof(state_id_t);
		base->blocks[2].offset = align_to_64(base->blocks[1].endpos());
		base->blocks[2].length = m_index_map.size() * sizeof(IndexMapEntry);
		dataPtrs[0] = states.data();
		dataPtrs[1] = m_roots.data();
		dataPtrs[2] = m_index_map.data();

		return 0;
	}

	void print_vm_stat(FILE* fp) const {
		febitvec       color(states.size());
		valvec<size_t> stack(512, valvec_reserve());
		terark::AutoFree<size_t> dests(256);
		terark::AutoFree<unsigned> op_count(32, 0);
		terark::AutoFree<unsigned> op_slots(32, 0);
		for(size_t j = 0; j < m_roots.size(); ++j) {
			size_t root = m_roots[j];
			if (color.is1(root))
				continue;
			color.set1(root);
			stack.push(root);
			while (!stack.empty()) {
				size_t curr = stack.pop_val();
				assert(curr < states.size());
				const State* u = states.data() + curr;
				op_count[u->op] += 1;
				op_slots[u->op] += instruction_slots(u);
				this->pfs_put_children(curr, color, stack, dests.p);
			}
		}
		for (unsigned iop = 0; iop < OpCode_last; ++iop) {
			fprintf(fp, "op[%02d]: %-26s | cnt=%7u | len=%8u\n",
				iop, get_op_name(OpCode(iop)), op_count[iop], op_slots[iop]);
		}
		fprintf(fp, "\n");
		for (size_t map_id = 0; map_id < m_index_map.size(); ++map_id) {
			fprintf(fp
				, "map_index[%02zd]: chars = %3u uniq = %2u, refcnt = %u\n"
				, map_id
				, m_index_map[map_id].n_char
				, m_index_map[map_id].n_uniq
				, m_index_map[map_id].refcnt
				);
		}
		fprintf(fp, "\n");
	}
};

template<class DFA>
size_t dfa_matchid_root(const DFA* dfa, size_t state)
{ return dfa->state_move(state, FULL_MATCH_DELIM); }

///@param Collector requires .append(const int*, num)
///                          .push_back(int)
///                          .size()
///  (*vec)[i] != (*vec)[i+1]
template<class DFA, class Collector>
size_t dfa_read_matchid(const DFA* dfa, size_t state, Collector* vec)
{ return dfa_read_binary_int32(*dfa, state, vec); }

template<class Collector>
size_t dfa_read_matchid(const VirtualMachineDFA* dfa, size_t state, Collector* vec)
{ return dfa->read_matchid(state, vec); }

size_t dfa_matchid_root(const VirtualMachineDFA* dfa, size_t state)
{ return dfa->is_term(state) ? state : dfa->nil_state; }

} // namespace terark
