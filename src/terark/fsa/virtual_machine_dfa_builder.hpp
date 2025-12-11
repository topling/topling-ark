#pragma once

#include "virtual_machine_dfa.hpp"
#include <terark/hash_strmap.hpp>
#include <terark/sso.hpp>
#include <terark/succinct/rank_select_simple.hpp>

namespace terark {

template<class T>
struct StaticSparseArray {
	rank_select_simple  idx;
	valvec<T>           val;

	const T& operator[](size_t i) const {
		assert(i < idx.size());
		size_t j = idx.rank1(i);
		return val[j];
	}
	T& operator[](size_t i) {
		assert(i < idx.size());
		size_t j = idx.rank1(i);
		return val[j];
	}

	StaticSparseArray(size_t n) : idx(n) {}
};

/// an optimized map<uint, vec<T> >
///
template<class T>
struct StaticMultiSparseArray {
	rank_select_simple  rank;
	valvec<unsigned>    dpos;
	valvec<T>           data;

	void get_range(size_t key, const T** beg, size_t* len) const {
		assert(key < rank.size());
		if (rank.is1(key)) {
			size_t i = rank.rank1(key);
			size_t s = dpos[i + 0];
			size_t t = dpos[i + 1];
			assert(s < t);
			*beg = data.data() + s;
			*len = t - s;
		}
		else {
			*beg = NULL;
			*len = 0;
		}
	}
	void complete() {
		dpos.push(data.size());
		dpos.shrink_to_fit();
		data.shrink_to_fit();
		rank.build_cache(false, false);
	}
	StaticMultiSparseArray(size_t n) : rank(n) {}
};

struct IndirectIndex {
	static const size_t NumWords = 256 / sizeof(size_t);
	union {
		size_t m_words[NumWords];
		byte_t m_index[256];
	};
	void reset() {
		memset(m_index, -1, 256);
	}
	size_t get(size_t ch) const {
		assert(ch < 256);
		return m_index[ch];
	}
	void set(size_t ch, size_t val) {
		assert(val < 128);
		m_index[ch] = val;
	}
	struct Hash {
		size_t operator()(const IndirectIndex& x) const {
			size_t hval = 12345;
			for (size_t i = 0; i < NumWords; ++i) {
				hval = FaboHashCombine(hval, x.m_words[i]);
			}
			return hval;
		}
	};
	struct Equal{
		bool operator()(const IndirectIndex& x, const IndirectIndex& y) const {
			for (size_t i = 0; i < NumWords; ++i) {
				if (x.m_words[i] != y.m_words[i])
					return false;
			}
			return true;
		}
	};
};
struct IndirectRefcnt {
	uint32_t real_refcnt() const {
		return loop_cnt_real + non_loop_cnt_real;
	}
	uint32_t loop_cnt = 0;
	uint32_t non_loop_cnt = 0;
	uint32_t loop_cnt_real = 0;
	uint32_t non_loop_cnt_real = 0;
};
struct DirectRefcnt {
	uint32_t refcnt = 0;
	uint32_t map_id = UINT32_MAX;
};

class VirtualMachineDFA::Builder {
public:
	template<class Au>
	explicit Builder(const Au& au, valvec<State>* states);

	template<class Au, class AuID>
	void build(VirtualMachineDFA& self, const Au& au
		, const AuID* src_roots, size_t num_roots
		, state_id_t* dst_roots);

	size_t m_gnode_states;

private:
	terark::AutoFree<CharTarget<size_t> > moves;
	terark::AutoFree<size_t>  dests;
#ifndef NDEBUG
	terark::AutoFree<CharTarget<size_t> > temp1;
	terark::AutoFree<uint32_t>  dests_map;
	terark::AutoFree<uint32_t>  moves_map;
#endif
	terark::AutoFree<uint32_t>  idmap;
	StaticMultiSparseArray<int> capture;
	StaticMultiSparseArray<int> matchid;
	terark::AutoFree<unsigned> m_op_cnt;
	terark::AutoFree<unsigned> m_op_len;
	gold_hash_map<uint32_t, uint32_t> freq_map; // target -> freq

	gold_hash_map< IndirectIndex, IndirectRefcnt
				 , IndirectIndex::Hash
				 , IndirectIndex::Equal
				 > m_iimap;
	gold_hash_tab<u32, u32> m_serialset;
	IndirectIndex m_iikey;

	valvec<u32> stack;
	febitvec    color;
	valvec<State>* m_states;
	size_t  t1, t2;
	size_t  n1, n2, n3;
	size_t  n_move; // moves with label value 0~257
	size_t  n_char; // moves with label value 0~255
	size_t  n_uniq;
	size_t  nonloop_char;
	size_t  nonloop_dest;
	size_t  theloop_char;
	size_t  n_loop;
	size_t  i_slot; // instruction + transition slots
	size_t  z_slot; // zpath slots
	size_t  n_slot;
	size_t  m_src_states;
	fstring zstr;
	const int* p_capture;
	size_t     n_capture;
	const int* p_matchid;
	size_t     n_matchid;
	OpCode     op;
	int        m_show_stat;
	unsigned   m_neg_minch;
	unsigned   m_neg_maxch;

	size_t range() const {
		return n_char ? moves[n_char-1].ch - moves[0].ch + 1 : 0;
	}
	byte_t minch() const { return n_char ? moves[0].ch : 0; }
	byte_t maxch() const { return n_char ? moves[n_char-1].ch : 0; }

	void discover_children();
	size_t matchid_slots(int inline_id_bits) const;

	template<class Au>
	void compute(const Au& au, size_t state) {
		assert(UINT32_MAX != idmap[state]);
		n_move = au.get_all_move(state, moves.p);
		zstr = au.is_pzip(state) ? au.get_zpath_data(state, NULL) : "";
		compute_aux(state);
	#ifndef NDEBUG
		for (size_t j = 0; j < n_char; ++j) moves_map[j] = idmap[moves[j].target];
		for (size_t j = 0; j < n_uniq; ++j) dests_map[j] = idmap[dests[j]];
	#endif
		select_instruction(state);
	}
	template<class Au>
	void compute_iikey(const Au& au, size_t state) {
		n_move = au.get_all_move(state, moves.p);
		zstr = au.is_pzip(state) ? au.get_zpath_data(state, NULL) : "";
		compute_aux(state);
	}
	void compute_aux(size_t state);
	void select_instruction(size_t state);
	void emit_state(const VirtualMachineDFA& self, size_t state) const;
	template<class Au>
	void check_state(const VirtualMachineDFA& self, const Au& au, size_t state) const;

	bool reduce_pointer_width(VirtualMachineDFA& self, state_id_t* dst_roots, size_t num_roots, bool enable_direct_map);
};

template<class Au>
VirtualMachineDFA::Builder::Builder(const Au& au, valvec<State>* states)
  : moves(au.get_sigma())
  , dests(au.get_sigma())
#ifndef NDEBUG
  , temp1(au.get_sigma())
  , dests_map(au.get_sigma())
  , moves_map(au.get_sigma())
#endif
  , idmap(au.total_states(), UINT32_MAX)
  , capture(au.total_states())
  , matchid(au.total_states())
  , m_op_cnt(32, 0)
  , m_op_len(32, 0)
  , stack(128, valvec_reserve())
  , color(au.total_states(), valvec_no_init())
  , m_states(states)
{
	m_src_states = au.total_states();
	m_show_stat = 0;
	if (char* env = getenv("STATIC_DENSE_DFA_BUILDER_SHOW_STAT")) {
		m_show_stat = atoi(env);
	}
	if (Au::sigma > 256) {
		capture.dpos.reserve(au.total_states() / 8);
		capture.data.reserve(au.total_states() / 8);
	}
	matchid.dpos.reserve(au.total_states() / 8);
	matchid.data.reserve(au.total_states() / 8);
	for(size_t i = 0; i < au.total_states(); ++i) {
		assert(!au.is_free(i));
		if (Au::sigma > 256) {
			size_t s = au.state_move(i, SUB_MATCH_DELIM);
			if (au.nil_state != s) {
				capture.rank.set1(i);
				capture.dpos.push(capture.data.size());
				size_t oldsize = capture.data.size();
				size_t n = dfa_read_binary_int32(au, s, &capture.data);
				sort_0(capture.data.data() + oldsize, n);
				if (m_show_stat >= 3)
					printf("state[%05zd]: n_caputure=%zd\n", i, n);
			}
		}
		size_t f = dfa_matchid_root(&au, i);
		if (au.nil_state != f) {
			matchid.rank.set1(i);
			matchid.dpos.push(matchid.data.size());
			size_t oldsize = matchid.data.size();
			size_t n = dfa_read_matchid(&au, f, &matchid.data);
			int* d = matchid.data.data() + oldsize;
			sort_0(d, n);
			if (m_show_stat >= 3)
				printf("state[%05zd]: matchid: %s\n", i, *ssojoin(d, n));
		}
	}
	capture.complete();
	matchid.complete();
#if !defined(NDEBUG)
	for(size_t i = 0; i < matchid.dpos.size()-1; ++i) {
		size_t beg = matchid.dpos[i+0];
		size_t end = matchid.dpos[i+1];
		assert(beg < end);
		size_t select1 = matchid.rank.select1(i);
		size_t rank1 = matchid.rank.rank1(select1);
		assert(rank1 == i);
		for (size_t j = beg; j < end-1; ++j) {
			assert(matchid.data[j] != matchid.data[j+1]);
		}
	}
#endif
}

template<class Au, class AuID>
void VirtualMachineDFA::Builder::build(VirtualMachineDFA& self, const Au& au
		, const AuID* src_roots, size_t num_roots
		, state_id_t* dst_roots)
{
	self.set_is_dag(au.is_dag());
	self.set_kv_delim(au.kv_delim());

	// check for duplicated roots, duplication should be very rare
	color.fill(0);
	for(size_t j = 0; j < num_roots; ++j) {
		size_t root = src_roots[j];
		if (color.is1(root)) {
			THROW_STD(invalid_argument, "duplicate root=%zd", root);
		}
		color.set1(root);
	}
	// do not start with all roots into stack makes CPU cache friendly
	color.fill(0);
	for(size_t j = 0; j < num_roots; ++j) {
		size_t root = src_roots[j];
		if (color.is1(root))
			continue;
		color.set1(root);
		stack.push(root);
		while (!stack.empty()) {
			size_t state = stack.pop_val();
			compute_iikey(au, state);
			if (n_char >= 2) {
				if (n_loop)
					m_iimap[m_iikey].loop_cnt++;
				else
					m_iimap[m_iikey].non_loop_cnt++;
			}
			discover_children();
		}
	}
	size_t id = 0;
	color.fill(0);
	m_gnode_states = 0;
	for(size_t j = 0; j < num_roots; ++j) {
		size_t root = src_roots[j];
		if (color.is1(root))
			continue;
		color.set1(root);
		stack.push(root);
		while (!stack.empty()) {
			size_t state = stack.pop_val();
			capture.get_range(state, &p_capture, &n_capture);
			matchid.get_range(state, &p_matchid, &n_matchid);
			idmap[state] = id;
			compute(au, state);
			id += n_slot;
			m_gnode_states++;
			discover_children();
		}
	}
	if (id >= max_state) {
		THROW_STD(out_of_range, "id=%zd exceeds max_state=%zd"
				, id, (size_t)max_state);
	}

	if (m_show_stat >= 2)
		printf("before erase: m_iimap.end_i() = %7d\n", (int)m_iimap.end_i());

	m_iimap.shrink_after_erase_if(
		[](std::pair<IndirectIndex, IndirectRefcnt>& x) {
			return 0 == x.second.real_refcnt();
		});

	if (m_show_stat >= 2)
		printf("after  erase: m_iimap.end_i() = %7d\n", (int)m_iimap.end_i());

	self.m_index_map.resize_no_init(m_iimap.end_i());
	for(size_t map_id = 0; map_id < m_iimap.end_i(); ++map_id) {
		size_t max_ch_id = 0;
		size_t num_chars = 0;
		for(size_t ch = 0; ch < 256; ++ch) {
			size_t ch_id = m_iimap.key(map_id).get(ch);
			self.m_index_map[map_id].index[ch] = ch_id;
			if (ch_id < 255) {
				max_ch_id = std::max(max_ch_id, ch_id);
				num_chars++;
			}
		}
		self.m_index_map[map_id].n_uniq = max_ch_id + 1;
		self.m_index_map[map_id].n_char = num_chars;
		self.m_index_map[map_id].refcnt = m_iimap.val(map_id).real_refcnt();

		// for next round calling of select_instruction:
		m_iimap.val(map_id).loop_cnt_real = 0;
		m_iimap.val(map_id).non_loop_cnt_real = 0;
	}

	m_states->resize_no_init(id);
	BOOST_STATIC_ASSERT(sizeof(state_t) == 4);
	State* pstates = m_states->data();
	memset(pstates, 0, sizeof(state_t) * id);
	id = 0;
	color.fill(0);
	for(size_t j = 0; j < num_roots; ++j) {
		size_t root = src_roots[j];
		dst_roots[j] = idmap[root];
		if (color.is1(root))
			continue;
		color.set1(root);
		stack.push(root);
		while (!stack.empty()) {
			size_t state = stack.pop_val();
			assert(id == idmap[state]);
			capture.get_range(state, &p_capture, &n_capture);
			matchid.get_range(state, &p_matchid, &n_matchid);
			compute(au, state);
			emit_state(self, state);
			check_state(self, au, state);
			id += n_slot;
			discover_children();
		}
	}
#ifndef NDEBUG
	for (size_t j = 0; j < au.total_states(); ++j) {
		if (UINT32_MAX != idmap[j])
			check_state(self, au, j);
	}
#endif

	// optimize ANY BYTE sequences into Inst_all_char_all_jump
	// this will enabling fast jump without read input bytes
	// a.{1000}b, when entering ".{1000}", it can do fast jump
	color.resize_fill(id, 0);

	for(size_t j = 0; j < num_roots; ++j) {
		size_t root = dst_roots[j];
		assert(root < id);
		if (color.is1(root))
			continue;
		color.set1(root);
		stack.push(root);
		while (!stack.empty()) {
			size_t curr = stack.pop_val();
			size_t n = 0;
			assert(curr < id);
			auto f = (Inst_mul_char_one_diff_fullfill*)(pstates + curr);
			if (mul_char_one_diff_fullfill == f[n].op
			   && 256 == f[n].range() && 1 == f[n].diff_val) {
				do {
					assert(curr + n < id-1);
					++n;
				} while (mul_char_one_diff_fullfill == f[n].op
						&& 256 == f[n].range() && 1 == f[n].diff_val
						&& color.is0(curr + n));
				if (m_show_stat >= 3)
					printf("found a max_jump = %zd, state = %zd\n", n, curr);
			}
			auto p = (Inst_all_char_all_jump*)(f);
			for(size_t k = 0; k < n; ++k) {
				p[k].op = all_char_all_jump;
				p[k].max_jump = n - k;
				color.set1(curr + k);
			}
			size_t oldsize = stack.size();
			size_t cnt = self.get_all_dest(curr + n, dests);
			for(size_t k = 0; k < cnt; ++k) {
				size_t child = dests[k];
				if (color.is0(child)) {
					color.set1(child);
					stack.push(child);
				}
			}
			reverse_a(stack, oldsize, stack.size());
		}
	}

	if (m_show_stat >= 3) {
		self.print_vm_stat(stdout);
	}

	if (m_show_stat >= 5) {
		for(size_t j = 0; j < num_roots; ++j) {
			size_t root = dst_roots[j];
			char  fname[64];
			sprintf(fname, "vmdfa/%zd.dot", j);
			Au dfa1;
			dfa1.copy_from(initial_state, self, root);
			dfa1.write_dot_file(fname);
		}
	}

#ifndef NDEBUG
	for (size_t j = 0; j < au.total_states(); ++j) {
		if (UINT32_MAX != idmap[j])
			check_state(self, au, j);
	}
#endif

	for (size_t j = 0; j < 4; ++j) {
		if (m_show_stat >= 2) {
			printf("reduce_pointer_width: round %d, states = %9d, index_map = %7d\n"
					, (int)j, (int)m_states->size()
					, (int)self.m_index_map.size()
					);
		}
		if (m_show_stat >= 3) {
			self.print_vm_stat(stdout);
		}
		if (!reduce_pointer_width(self, dst_roots, num_roots, false))
			break;
	#ifndef NDEBUG
		for (size_t k = 0; k < m_src_states; ++k) {
			if (UINT32_MAX != idmap[k])
				check_state(self, au, k);
		}
	#endif
	}

	if (m_show_stat >= 2) {
		printf("reduce_pointer_width: round 4, states = %9d, index_map = %7d\n"
				, (int)m_states->size()
				, (int)self.m_index_map.size()
				);
		if (m_show_stat >= 3) {
			self.print_vm_stat(stdout);
		}
		printf("\n");
		printf("reduce_pointer_width: optimizing by direct map...\n");

		reduce_pointer_width(self, dst_roots, num_roots, true);

		printf("reduce_pointer_width: completed, states = %9d, index_map = %7d\n\n"
				, (int)m_states->size()
				, (int)self.m_index_map.size()
				);
		if (m_show_stat >= 3) {
			self.print_vm_stat(stdout);
		}
	}
	else {
		reduce_pointer_width(self, dst_roots, num_roots, true);
	}

	self.m_total_zpath_len = au.total_zpath_len();
	self.m_zpath_states = au.num_zpath_states();
	self.m_transition_num = au.total_transitions();
}

inline // this `inline' is just for producing weak symbol
bool VirtualMachineDFA::Builder::reduce_pointer_width
	(VirtualMachineDFA& self, state_id_t* dst_roots, size_t num_roots, bool enable_direct_map)
{
	size_t ip1 = 0, ip2 = 0;
	valvec<uint32_t> backpatch_chain(m_states->size(), UINT32_MAX);
	valvec<state_t> states2(m_states->size(), valvec_no_init());
	memset(states2.data(), 0, sizeof(state_t) * states2.size());

	febitvec color2(color.size(), false);
	state_t* states1 = m_states->data();

	// now `ipmap` is the ip map from `m_state` to `states2`
	AutoFree<uint32_t> ipmap(m_states->size(), UINT32_MAX);

	auto reduce_index_mapped = [&](OpCode op_i8, OpCode op_i16, size_t n_uniq1) {
		auto p1 = (Inst_mul_dest_index_mapped*)(states1 + ip1);
		intptr_t max_diff = 0, min_diff = 0;
		for(size_t i = 0; i < n_uniq1; ++i) {
			size_t dest1 = p1->targets[i];
			if (dest1 <= ip1) {
				assert(UINT32_MAX != ipmap[dest1]);
				intptr_t diff2 = intptr_t(ipmap[dest1]) - intptr_t(ip2);
				min_diff = std::min(min_diff, diff2);
			}
			else {
				intptr_t diff1 = intptr_t(dest1) - intptr_t(ip1);
				max_diff = std::max(max_diff, diff1);
			}
		}
		if (max_diff <= 127 && min_diff >= -128) {
			states2[ip2].op = op_i8;
			ip2 += 1 + ceiled_div(n_uniq1, 4);
		}
		else if (max_diff <= INT16_MAX && min_diff >= INT16_MIN) {
			states2[ip2].op = op_i16;
			ip2 += 1 + ceiled_div(n_uniq1, 2);
		}
		else {
			ip2 += 1 + n_uniq1;
		}
		ip1 += 1 + n_uniq1;
	};

	auto patch_index_mapped = [&](size_t n_uniq1) {
		auto p1 = (Inst_mul_dest_index_mapped*)(states1 + ip1);
		assert(ipmap[ip1] == ip2);
		switch (states2[ip2].op) {
		default:
			assert(0);
			break;
		case mul_dest_index_mapped_i8:
		case mul_loop_index_mapped_i8:
		  {
			auto p2 = (Inst_mul_dest_index_mapped_i8*)(&states2[ip2]);
			for(size_t i = 0; i < n_uniq1; ++i) {
				intptr_t dest1 = ipmap[p1->targets[i]];
				intptr_t diff1 = dest1 - intptr_t(ip2);
				p2->diffs_i8[i] = int8_t(diff1);
			}
			ip1 += 1 + n_uniq1;
			ip2 += 1 + ceiled_div(n_uniq1, 4);
			break;
		  }
		case mul_dest_index_mapped_i16:
		case mul_loop_index_mapped_i16:
		  {
			auto p2 = (Inst_mul_dest_index_mapped_i16*)(&states2[ip2]);
			for (size_t i = 0; i < n_uniq1; ++i) {
				intptr_t dest1 = ipmap[p1->targets[i]];
				intptr_t diff1 = dest1 - intptr_t(ip2);
				p2->diffs_i16[i] = int16_t(diff1);
			}
			ip1 += 1 + n_uniq1;
			ip2 += 1 + ceiled_div(n_uniq1, 2);
			break;
		  }
		case mul_dest_index_mapped:
		case mul_loop_index_mapped:
		  {
			auto p2 = (Inst_mul_dest_index_mapped*)(&states2[ip2]);
			for (size_t i = 0; i < n_uniq1; ++i) {
				p2->targets[i] = ipmap[p1->targets[i]];
			}
			ip1 += 1 + n_uniq1;
			ip2 += 1 + n_uniq1;
			break;
		  }
		}
	};

	auto reduce_index_mapped_i16 = [&](OpCode op_i8, size_t n_uniq1) {
		auto u1 = (state_t*)(states1 + ip1);
		auto p1 = (Inst_mul_dest_index_mapped_i16*)(u1);
		intptr_t max_diff = 0, min_diff = 0;
		for (size_t i = 0; i < n_uniq1; ++i) {
			intptr_t diff1 = p1->diffs_i16[i];
			if (diff1 <= 0) {
				intptr_t dest1 = intptr_t(ip1) + diff1;
				intptr_t diff2 = intptr_t(ipmap[dest1]) - intptr_t(ip2);
				assert(dest1 < intptr_t(m_states->size()));
				assert(UINT32_MAX != ipmap[dest1]);
				min_diff = std::min(min_diff, diff2);
			}
			else {
				max_diff = std::max(max_diff, diff1);
			}
		}
		if (max_diff <= 127 && min_diff >= -128) {
			states2[ip2].op = op_i8;
			ip2 += 1 + ceiled_div(n_uniq1, 4);
		}
		else {
			ip2 += 1 + ceiled_div(n_uniq1, 2);
		}
		ip1 += 1 + ceiled_div(n_uniq1, 2);
	};

	auto patch_index_mapped_i16 = [&](size_t n_uniq1) {
		auto p1 = (Inst_mul_dest_index_mapped_i16*)(states1 + ip1);
		switch (states2[ip2].op) {
		default:
			assert(0);
			break;
		case mul_dest_index_mapped_i8:
		case mul_loop_index_mapped_i8:
		  {
			auto p8 = (Inst_mul_dest_index_mapped_i8*)(&states2[ip2]);
			for (size_t i = 0; i < n_uniq1; ++i) {
				intptr_t dest1 = intptr_t(ip1) + p1->diffs_i16[i];
				intptr_t diff2 = intptr_t(ipmap[dest1]) - intptr_t(ip2);
				p8->diffs_i8[i] = int8_t(diff2);
			}
			ip1 += 1 + ceiled_div(n_uniq1, 2);
			ip2 += 1 + ceiled_div(n_uniq1, 4);
			break;
		  }
		case mul_dest_index_mapped_i16:
		case mul_loop_index_mapped_i16:
		  {
			auto p16 = (Inst_mul_dest_index_mapped_i16*)(&states2[ip2]);
			for (size_t i = 0; i < n_uniq1; ++i) {
				intptr_t dest1 = intptr_t(ip1) + p1->diffs_i16[i];
				intptr_t diff2 = intptr_t(ipmap[dest1]) - intptr_t(ip2);
				p16->diffs_i16[i] = int16_t(diff2);
			}
			ip1 += 1 + ceiled_div(n_uniq1, 2);
			ip2 += 1 + ceiled_div(n_uniq1, 2);
			break;
		  }
		}
	};

	hash_strmap<DirectRefcnt> drmap1, drmap2;

	auto has_neg_128 = [&]() {
		auto p1 = (Inst_mul_dest_index_mapped_i8*)(states1 + ip1);
		size_t n_uniq1 = self.m_index_map[p1->map_id].n_uniq;
		if (mul_loop_index_mapped_i8 == OpCode(p1->op))
			n_uniq1--;
		for (size_t j = 0; j < n_uniq1; ++j)
			if (-128 == p1->diffs_i8[j])
				return true;
		return false;
	};

	auto gen_drkey1 = [&](int8_t* drkey) {
		auto p1 = (Inst_mul_dest_index_mapped_i8*)(states1 + ip1);
		uint32_t map_id1 = p1->map_id;
		auto& me = self.m_index_map[map_id1];
		memcpy(drkey, &map_id1, 4);
		if (mul_loop_index_mapped_i8 == OpCode(p1->op)) {
			drkey[4] = 0;
			memcpy(drkey + 5, p1->diffs_i8, me.n_uniq - 1);
		}
		else {
			memcpy(drkey + 4, p1->diffs_i8, me.n_uniq);
		}
		return 4 + me.n_uniq;
	};

	auto gen_drkey2 = [&](int8_t* drkey) {
		auto p1 = (Inst_mul_dest_index_mapped_i8*)(states1 + ip1);
		uint32_t map_id1 = p1->map_id;
		auto& me = self.m_index_map[map_id1];
		size_t n_uniq1 = me.n_uniq;
		memcpy(drkey, &map_id1, 4);
		int8_t* beg = drkey + 4;
		if (mul_loop_index_mapped_i8 == OpCode(p1->op)) {
			n_uniq1--;
			*beg++ = 0;
		}
		for (size_t j = 0; j < n_uniq1; ++j) {
			// don't use `ip2`
			intptr_t dest1 = intptr_t(ip1) + p1->diffs_i8[j];
			intptr_t diff2 = intptr_t(ipmap.p[dest1]) - intptr_t(ipmap.p[ip1]);
			assert(diff2 >= -127 && diff2 <= 127);
			beg[j] = diff2;
		}
		return 4 + me.n_uniq;
	};

	if (enable_direct_map) {
		ip1 = 0;
		while (ip1 < m_states->size()) {
			auto u1 = states1 + ip1;
			switch (OpCode(u1->op)) {
			default:
				break;
			case mul_dest_index_mapped_i8:
			case mul_loop_index_mapped_i8:
				if (!has_neg_128()) {
					int8_t drkey[4 + 256]; // map_id + diff data
					drmap1[fstring(drkey, gen_drkey1(drkey))].refcnt++;
				}
				break;
			}
			do ++ip1;
			while (ip1 < m_states->size() && color.is0(ip1));
		}
	}

	ip1 = 0;
	// calculate width-reduced pointers:
	//
	while (ip1 < m_states->size()) {
		assert(ip2 < states2.size());
		assert(color.is1(ip1));
		ipmap[ip1] = ip2;
		states2[ip2] = states1[ip1]; // copy
		color2.set1(ip2);
		auto u1 = states1 + ip1;
		switch (OpCode(u1->op)) {
		default:
			++ip1;
			++ip2;
			break;
		case one_char_one_dest:
		case all_dest_one_loop:
		case all_loop_one_dest:
		  {
			auto p1 = (const Inst_one_char_one_dest*)(u1);
			intptr_t diff1 = intptr_t(p1->target) - intptr_t(ip1);
			if (!p1->term_bit && diff1 >= INT16_MIN && diff1 <= INT16_MAX) {
				states2[ip2].op
					= one_char_one_dest == u1->op ? one_char_one_diff
					: all_dest_one_loop == u1->op ? all_diff_one_loop
					: all_loop_one_diff;
				ip2 += 1;
			} else {
				ip2 += 2;
			}
			ip1 += 2;
			break;
		  }
		case mul_char_one_dest_fullfill:
		case neg_char_one_dest_fullfill:
		  {
			auto p1 = (const Inst_mul_char_one_dest_fullfill*)(u1);
			intptr_t diff1 = intptr_t(p1->target) - intptr_t(ip1);
			if (diff1 >= -128 && diff1 <= 127) {
				states2[ip2].op
					= mul_char_one_dest_fullfill == u1->op
					? mul_char_one_diff_fullfill
					: neg_char_one_diff_fullfill;
				ip2 += 1;
			} else {
				ip2 += 2;
			}
			ip1 += 2;
			break;
		  }
		case mul_dest_index_mapped:
		  {
			size_t map_id1 = ((BaseInst_index_mapped*)(u1))->map_id;
			size_t n_uniq1 = self.m_index_map[map_id1].n_uniq;
			reduce_index_mapped(mul_dest_index_mapped_i8,
								mul_dest_index_mapped_i16, n_uniq1);
			break;
		  }
		case mul_loop_index_mapped:
		  {
			size_t map_id1 = ((BaseInst_index_mapped*)(u1))->map_id;
			size_t n_uniq1 = self.m_index_map[map_id1].n_uniq - 1;
			reduce_index_mapped(mul_loop_index_mapped_i8,
								mul_loop_index_mapped_i16, n_uniq1);
			break;
		  }
		case mul_dest_index_mapped_i16:
		  {
			size_t map_id1 = ((BaseInst_index_mapped*)(u1))->map_id;
			size_t n_uniq1 = self.m_index_map[map_id1].n_uniq;
			reduce_index_mapped_i16(mul_dest_index_mapped_i8, n_uniq1);
			break;
		  }
		case mul_loop_index_mapped_i16:
		  {
			size_t map_id1 = ((BaseInst_index_mapped*)(u1))->map_id;
			size_t n_uniq1 = self.m_index_map[map_id1].n_uniq - 1;
			reduce_index_mapped_i16(mul_loop_index_mapped_i8, n_uniq1);
			break;
		  }
		case mul_dest_index_mapped_i8:
		case mul_loop_index_mapped_i8:
			if (enable_direct_map) {
				auto p1 = (Inst_mul_dest_index_mapped_i8*)(states1 + ip1);
				auto& me = self.m_index_map[p1->map_id];
				size_t n_uniq1 = me.n_uniq;
				if (mul_loop_index_mapped_i8 == u1->op)
					n_uniq1--;
				size_t extra = ceiled_div(n_uniq1, 4);
				size_t islots = 1 + extra;
				if (!has_neg_128()) {
					int8_t drkey[4 + 256];
					auto dr1 = &drmap1[fstring(drkey, gen_drkey1(drkey))];
					assert(dr1->refcnt > 0);
					if (dr1->refcnt == me.refcnt ||
							sizeof(IndexMapEntry) <= extra * dr1->refcnt) {
						states2[ip2].op = direct_mapped_i8;
						ip2 += 1;
					}
					else {
						ip2 += islots;
					}
				}
				else {
					ip2 += islots;
				}
				ip1 += islots;
			}
			else {
				++ip1;
				++ip2;
			}
			break;
		}
		while (ip1 < m_states->size() && color.is0(ip1))
			++ip1, ++ip2;
	}
	assert(m_states->size() == ip1);
	states2.resize(ip2);

	if (enable_direct_map) {
		// generate drmap2
		ip1 = 0;
		while (ip1 < m_states->size()) {
			assert(color.is1(ip1));
			assert(UINT32_MAX != ipmap.p[ip1]);
			auto u1 = states1 + ip1;
			switch (OpCode(u1->op)) {
			default:
				break;
			case mul_dest_index_mapped_i8:
			case mul_loop_index_mapped_i8:
				if (!has_neg_128()) {
					int8_t drkey[4 + 256]; // map_id + diff data
					drmap2[fstring(drkey, gen_drkey2(drkey))].refcnt++;
				}
				break;
			}
			do ++ip1;
			while (ip1 < m_states->size() && color.is0(ip1));
		}
	}

	gold_hash_map<size_t, IndexMapEntry> pending_write;

	// backpatch width-reduced pointers:
	//
	ip1 = 0;
	ip2 = 0;
	while (ip1 < m_states->size()) {
		assert(ip2 == ipmap[ip1]);
		assert(color.is1(ip1));
		auto u1 = states1 + ip1;
		switch (OpCode(u1->op)) {
		default:
		  {
			assert(u1->op == states2[ip2].op);
		//	size_t islots = self.instruction_slots(u1);
			ip1 += 1;
			ip2 += 1;
			break;
		  }
		case one_char_one_diff:
		case all_diff_one_loop:
		case all_loop_one_diff:
		  {
			state_id_t dest1 = state_id_t(intptr_t(ip1) + u1->diff_val);
			intptr_t   diff2 = intptr_t(ipmap[dest1]) - intptr_t(ip2);
			states2[ip2].diff_val = diff2;
			ip1 += 1;
			ip2 += 1;
			break;
		  }
		case one_char_one_dest:
		case all_dest_one_loop:
		case all_loop_one_dest:
		  {
			auto p1 = (const Inst_one_char_one_dest*)(u1);
			auto p2 = (      Inst_one_char_one_dest*)(&states2[ip2]);
			if (p2->op != p1->op) {
				assert(!p1->term_bit);
				states2[ip2].diff_val = intptr_t(ipmap[p1->target]) - intptr_t(ip2);
				ip2 += 1;
			}
			else {
				p2->target = ipmap[p1->target];
				ip2 += 2;
			}
			ip1 += 2;
			break;
		  }
		case mul_char_one_diff_fullfill:
		case neg_char_one_diff_fullfill:
		  {
			auto p1 = (const Inst_mul_char_one_diff_fullfill*)(u1);
			auto p2 = (      Inst_mul_char_one_diff_fullfill*)(&states2[ip2]);
			state_id_t dest1 = state_id_t(intptr_t(ip1) + p1->diff_val);
			intptr_t   diff2 = intptr_t(ipmap[dest1]) - intptr_t(ip2);
			p2->diff_val = diff2;
			ip1 += 1;
			ip2 += 1;
			break;
		  }
		case mul_char_one_dest_fullfill:
		case neg_char_one_dest_fullfill:
		  {
			auto p1 = (const Inst_mul_char_one_dest_fullfill*)(u1);
			if (p1->op != states2[ip2].op) {
				auto p2 = (Inst_mul_char_one_diff_fullfill*)(&states2[ip2]);
				p2->diff_val = intptr_t(ipmap[p1->target]) - intptr_t(ip2);
				ip2 += 1;
			}
			else {
				auto p2 = (Inst_mul_char_one_dest_fullfill*)(&states2[ip2]);
				p2->target = ipmap[p1->target];
				ip2 += 2;
			}
			ip1 += 2;
			break;
		  }
		case mul_char_one_dest_small_bm:
		  {
			auto p1 = (const Inst_mul_char_one_dest_small_bm*)(u1);
			auto p2 = (      Inst_mul_char_one_dest_small_bm*)(&states2[ip2]);
			p2->target = ipmap[p1->target];
			ip1 += 2;
			ip2 += 2;
			break;
		  }
		case mul_char_one_dest_large_bm:
		  {
			auto p1 = (const Inst_mul_char_one_dest_large_bm*)(u1);
			auto p2 = (      Inst_mul_char_one_dest_large_bm*)(&states2[ip2]);
			unsigned nbm = p1->nbm + 1;
			memcpy(p2->tarbits, p1->tarbits, 4 * nbm);
			p2->tarbits[nbm] = ipmap[p1->tarbits[nbm]];
			ip1 += 2 + nbm;
			ip2 += 2 + nbm;
			break;
		  }
		case mul_char_two_dest_fullfill:
		  {
			auto p1 = (const Inst_mul_char_two_dest_fullfill*)(u1);
			auto p2 = (      Inst_mul_char_two_dest_fullfill*)(&states2[ip2]);
			size_t nbm = p1->calc_nbm();
			memcpy(p2->tarbits, p1->tarbits, 4 * nbm);
			p2->tarbits[nbm + 0] = ipmap[p1->tarbits[nbm + 0]];
			p2->tarbits[nbm + 1] = ipmap[p1->tarbits[nbm + 1]];
			ip1 += 3 + nbm;
			ip2 += 3 + nbm;
			break;
		  }
		case mul_loop_one_dest_fullfill:
		  {
			auto p1 = (const Inst_mul_loop_one_dest_fullfill*)(u1);
			auto p2 = (      Inst_mul_loop_one_dest_fullfill*)(&states2[ip2]);
			size_t nbm = p1->calc_nbm();
			memcpy(p2->tarbits, p1->tarbits, 4 * nbm);
			p2->tarbits[nbm] = ipmap[p1->tarbits[nbm]];
			ip1 += 2 + nbm;
			ip2 += 2 + nbm;
			break;
		  }
		case mul_char_2o3_dest_small_bm:
		case mul_loop_1o2_dest_small_bm:
		  {
			auto p1 = (const Inst_mul_char_2o3_dest_small_bm*)(u1);
			auto p2 = (      Inst_mul_char_2o3_dest_small_bm*)(&states2[ip2]);
			unsigned b = p1->bits1;
			unsigned n = (b & (b >> 1) & BitMask32) ? 3 : 2;
			if (mul_loop_1o2_dest_small_bm == u1->op)
				n--;
			for (unsigned j = 0; j < n; ++j) {
				p2->targets[j] = ipmap[p1->targets[j]];
			}
			ip1 += 1 + n;
			ip2 += 1 + n;
			break;
		  }
		case mul_char_2o3_dest_large_bm:
		case mul_loop_1o2_dest_large_bm:
		  {
			auto p1 = (const Inst_mul_char_2o3_dest_large_bm*)(u1);
			auto p2 = (      Inst_mul_char_2o3_dest_large_bm*)(&states2[ip2]);
			unsigned nbm = p1->nbm + 1;
			unsigned n = 2;
			for(unsigned i = 0; i < nbm; ++i) {
				uint32_t b = p1->tarbits[i];
				if (b & (b >> 1) & BitMask32) {
					n = 3;
					break;
				}
			}
			if (2 == n) {
				unsigned b = p1->bits1;
				if (b & (b >> 1) & BitMask32)
					n = 3;
			}
			if (mul_loop_1o2_dest_large_bm == u1->op)
				n--;
			memcpy(p2->tarbits, p1->tarbits, 4 * nbm);
			for (unsigned j = 0; j < n; ++j) {
				p2->tarbits[nbm+j] = ipmap[p1->tarbits[nbm+j]];
			}
			ip1 += 1 + nbm + n;
			ip2 += 1 + nbm + n;
			break;
		  }
		case mul_char_mul_dest_small_bm:
		  {
			auto p1 = (const Inst_mul_char_mul_dest_small_bm*)(u1);
			auto p2 = (      Inst_mul_char_mul_dest_small_bm*)(&states2[ip2]);
			unsigned b = p1->bits1;
			unsigned n = fast_popcount32(b & (b >> 1) & BitMask32);
			p2->target1 = ipmap[p1->target1];
			p2->target2 = ipmap[p1->target2];
			for (unsigned j = 0; j < n; ++j)
				p2->targets[j] = ipmap[p1->targets[j]];
			ip1 += 3 + n;
			ip2 += 3 + n;
			break;
		  }
		case mul_char_mul_dest_large_bm:
		  {
			auto p1 = (const Inst_mul_char_mul_dest_large_bm*)(u1);
			auto p2 = (      Inst_mul_char_mul_dest_large_bm*)(&states2[ip2]);
			unsigned ch0 = p1->minch;
			unsigned nbm = ((p1->maxch - ch0 + 1) * 2 + 63) / 64 * 2;
			unsigned n = p1->r_sum + 1;
			auto ts1 = p1->ubits.bm32 + nbm;
			auto ts2 = p2->ubits.bm32 + nbm;
			p2->target1 = ipmap[p1->target1];
			p2->target2 = ipmap[p1->target2];
			memcpy(p2->rank, p1->rank, 8);
			memcpy(p2->ubits.bm32, p1->ubits.bm32, 4 * nbm);
			for (unsigned j = 0; j < n; ++j)
				ts2[j] = ipmap[ts1[j]];
			unsigned islots = 5 + p1->r_sum + 1 + nbm;
			ip1 += islots;
			ip2 += islots;
			break;
		  }
		case mul_char_mul_dest_huge4_bm:
		case mul_loop_mul_dest_huge4_bm: // nUniq doesn't include loop target
		  {
			auto p1 = (const Inst_mul_char_mul_dest_huge4_bm*)(u1);
			auto p2 = (      Inst_mul_char_mul_dest_huge4_bm*)(&states2[ip2]);
			size_t nbm = ((p1->maxch - p1->minch + 1) * 4 + 31) / 32;
			memcpy(p2->tarbits, p1->tarbits, 4 * nbm);
			for (unsigned j = 0; j < p1->nUniq; ++j) {
				p2->tarbits[nbm + j] = ipmap[p1->tarbits[nbm + j]];
			}
			ip1 += 1 + nbm + p1->nUniq;
			ip2 += 1 + nbm + p1->nUniq;
			break;
		  }
		case mul_dest_index_mapped:
		  {
			size_t map_id1 = ((Inst_mul_dest_index_mapped*)(u1))->map_id;
			size_t n_uniq1 = self.m_index_map[map_id1].n_uniq;
			patch_index_mapped(n_uniq1);
			break;
		  }
		case mul_loop_index_mapped:
		  {
			size_t map_id1 = ((Inst_mul_loop_index_mapped*)(u1))->map_id;
			size_t n_uniq1 = self.m_index_map[map_id1].n_uniq - 1;
			patch_index_mapped(n_uniq1);
			break;
		  }
		case mul_dest_index_mapped_i16:
		  {
			size_t map_id1 = ((Inst_mul_dest_index_mapped_i16*)(u1))->map_id;
			size_t n_uniq1 = self.m_index_map[map_id1].n_uniq;
			patch_index_mapped_i16(n_uniq1);
			break;
		  }
		case mul_loop_index_mapped_i16:
		  {
			size_t map_id1 = ((Inst_mul_loop_index_mapped_i16*)(u1))->map_id;
			size_t n_uniq1 = self.m_index_map[map_id1].n_uniq - 1;
			patch_index_mapped_i16(n_uniq1);
			break;
		  }
		case mul_dest_index_mapped_i8:
		case mul_loop_index_mapped_i8:
		  {
			auto p1 = ((Inst_mul_dest_index_mapped_i8*)(u1));
			auto p2 = ((Inst_mul_dest_index_mapped_i8*)(&states2[ip2]));
			size_t n_uniq1 = self.m_index_map[p1->map_id].n_uniq;
			if (mul_loop_index_mapped_i8 == p1->op)
				n_uniq1--;
			if (direct_mapped_i8 == p2->op) {
				assert(enable_direct_map);
				assert(!has_neg_128());
				auto& me = self.m_index_map[p1->map_id];
				size_t extra = ceiled_div(n_uniq1, 4);
				size_t islots = 1 + extra;
				DirectRefcnt *dr2 = NULL;
			#ifndef NDEBUG
				DirectRefcnt *dr1 = NULL;
				{
					int8_t drkey[4 + 256];
					dr1 = &drmap1[fstring(drkey, gen_drkey1(drkey))];
					assert(dr1->refcnt > 0);
				}
				assert(dr1->refcnt == me.refcnt || sizeof(IndexMapEntry) <= dr1->refcnt*extra);
			#endif
				{
					int8_t drkey[4 + 256];
					dr2 = &drmap2[fstring(drkey, gen_drkey2(drkey))];
					assert(dr2->refcnt > 0);
					assert(dr2->refcnt <= me.refcnt);
				}
				IndexMapEntry me2;
				me2.refcnt = dr2->refcnt;
				me2.n_char = me.n_char;
				me2.n_uniq = me.n_uniq;
				memset(me2.index, -128, sizeof(me2.index));
				for(size_t ch = 0; ch < 256; ++ch) {
					size_t ch_id = me.index[ch];
					if (255 != ch_id) {
						if (mul_loop_index_mapped_i8 == u1->op) {
							if (0 == ch_id) {
								me2.index[ch] = 0;
							} else {
								ch_id--;
								goto SetDiff;
							}
						} else { SetDiff:
							intptr_t diff1 = p1->diffs_i8[ch_id];
							intptr_t dest1 = intptr_t(ip1) + diff1;
							intptr_t dest2 = intptr_t(ipmap[dest1]);
							intptr_t diff2 = dest2 - intptr_t(ip2);
							assert(diff2 >= -127 && diff2 <= 127);
							me2.index[ch] = diff2;
						}
					}
				}
				if (dr2->refcnt == me.refcnt) {
					pending_write.insert_i(p1->map_id, me2);
				}
				else {
					// TODO: Fix `me.refcnt`
					if (UINT32_MAX == dr2->map_id) {
						dr2->map_id = self.m_index_map.size();
						self.m_index_map.push_back(me2);
					}
					p2->map_id = dr2->map_id;
				}
				ip1 += islots;
				ip2 += 1;
			}
			else {
				assert(p1->op == p2->op);
				for(size_t j = 0; j < n_uniq1; ++j) {
					size_t dest1 = intptr_t(ip1) + p1->diffs_i8[j];
					p2->diffs_i8[j] = intptr_t(ipmap[dest1]) - intptr_t(ip2);
				}
				ip1 += 1 + ceiled_div(n_uniq1, 4);
				ip2 += 1 + ceiled_div(n_uniq1, 4);
			}
			break;
		  }
		}
		while (ip1 < m_states->size() && color.is0(ip1)) {
			states2[ip2] = states1[ip1];
			++ip1; ++ip2;
		}
	}
	assert(m_states->size() == ip1);
	assert(states2.size() == ip2);

	for(size_t j = 0; j < pending_write.end_i(); ++j) {
		size_t map_id = pending_write.key(j);
		auto& me1 = self.m_index_map[map_id];
		auto& me2 = pending_write.val(j);
		memcpy(me1.index, me2.index, 256);
	}

	for (size_t j = 0; j < num_roots; ++j) {
		assert(ipmap.p[dst_roots[j]] < ip2);
		dst_roots[j] = ipmap.p[dst_roots[j]];
	}

	color.swap(color2);
	m_states->swap(states2);

#ifndef NDEBUG
	AutoFree<uint32_t> idmap2(m_src_states, UINT32_MAX);
	for (size_t j = 0; j < m_src_states; ++j) {
		if (UINT32_MAX != idmap.p[j])
			idmap2.p[j] = ipmap.p[idmap.p[j]];
	}
	idmap.swap(idmap2);
#endif

	return ip2 < ip1;
}

template<class Au>
void
VirtualMachineDFA::Builder::
check_state(const VirtualMachineDFA& self, const Au& au, size_t state) const {
#ifndef NDEBUG
	size_t ip = idmap[state];
	size_t idx1 = 0, idx2 = 0;
	self.for_each_move(ip, [&](size_t t, size_t ch) {
		size_t t2 = self.state_move(ip, ch);
		assert(t2 < self.total_states());
		assert(t2 == t);
		temp1.p[idx1].ch = ch;
		temp1.p[idx1].target = t2;
		idx1++;
	});
	au.for_each_move(state, [&](size_t t, size_t ch) {
		if (ch < 256) {
			size_t t2 = au.state_move(state, ch);
			assert(t2 == t);
			assert(idmap.p[t2] < self.total_states());
			assert(idmap.p[t2] == temp1.p[idx2].target);
			assert(ch == temp1.p[idx2].ch);
			idx2++;
		}
	});
	assert(idx1 == idx2);
	size_t nm1;
	const int* pm1;
	matchid.get_range(state, &pm1, &nm1);
	if (nm1) {
		assert(dfa_matchid_root(&au, state) != Au::nil_state);
		assert(self.is_term(ip));
		valvec<int> vec;
		size_t nm2 = self.read_matchid(ip, &vec);
		assert(nm2 == vec.size());
		assert(nm2 == nm1);
		for (size_t i = 0; i < nm2; ++i) {
			assert(vec[i] == pm1[i]);
		}
	}
	else {
		assert(dfa_matchid_root(&au, state) == Au::nil_state);
		assert(!self.is_term(ip));
	}
	size_t nc1;
	const int* pc1;
	capture.get_range(state, &pc1, &nc1);
	if (nc1) {
		assert(self.has_capture(ip));
		const byte_t* pc2 = self.get_capture(ip);
		size_t nc2 = *pc2++ + 1;
		assert(nc2 == nc1);
		for (size_t i = 0; i < nc2; ++i) {
			assert(pc2[i]+2 == pc1[i]);
		}
	}
	else if (Au::sigma > 256) {
		assert(au.state_move(state, SUB_MATCH_DELIM) == Au::nil_state);
		assert(!self.has_capture(ip));
	}
	if (au.is_pzip(state)) {
		assert(self.is_pzip(ip));
		fstring zstr1 = au.get_zpath_data(state, NULL);
		fstring zstr2 = self.get_zpath_data(ip, NULL);
		assert(zstr1 == zstr2);
	}
	else {
		assert(!self.is_pzip(ip));
	}
#endif
}

inline
void VirtualMachineDFA::Builder::discover_children() {
	size_t oldsize = stack.size();
	for(size_t j = 0; j < n_char; ++j) {
		size_t child = moves[j].target;
		if (color.is0(child)) {
			color.set1(child);
			stack.push(child);
		}
	}
	reverse_a(stack, oldsize, stack.size());
}

inline
void VirtualMachineDFA::Builder::compute_aux(size_t state) {
	n_char = lower_bound_ex_0(moves.p, n_move, 256, CharTarget_By_ch());
	for (size_t j = 0; j < n_char; ++j)
		dests[j] = moves[j].target;
	sort_0(dests.p, n_char);
	t1 = size_t(-1);
	t2 = size_t(-1);
	n1 = 0;
	n2 = 0;
	n_uniq = 0;
	for(size_t j = 0; j < n_char; ) {
		size_t k = j;
		do ++k; while (k < n_char && dests[k] == dests[j]);
		assert(n2 <= n1);
		if (n1 < k-j) {
			n2 = n1;
			t2 = t1;
			n1 = k-j;
			t1 = dests[j];
		}
		else if (n2 < k-j) {
			n2 = k-j;
			t2 = dests[j];
		}
		dests[n_uniq++] = dests[j];
		j = k;
	}
	nonloop_char = size_t(-1);
	nonloop_dest = size_t(-1);
	theloop_char = size_t(-1);
	n_loop = 0;
	for(size_t j = 0; j < n_char; ++j) {
		if (moves[j].target != state) {
			nonloop_char = moves[j].ch;
			nonloop_dest = moves[j].target;
			break;
		}
	}
	for(size_t j = 0; j < n_char; ++j) {
		if (moves[j].target == state) {
			theloop_char = moves[j].ch;
			n_loop++;
		}
	}
	n3 = n_char - n1 - n2;

	if (n_char >= 2) {
		m_iikey.reset();
		m_serialset.erase_all();
		if (n_loop)
			m_serialset.insert_i(state);
		for(size_t j = 0; j < n_char; ++j) {
			size_t i = m_serialset.insert_i(moves[j].target).first;
			m_iikey.set(moves[j].ch, i);
		}
	}

	freq_map.erase_all();
	for(size_t j = 0; j < n_char; ++j)
		if (moves[j].target != state)
			freq_map[moves[j].target]++;
	assert(freq_map.end_i() + (n_loop?1:0) == n_uniq);
	typedef std::pair<u32,u32> puu;
	freq_map.sort([](puu x, puu y){return x.second < y.second;});

	m_neg_minch = 0;
	m_neg_maxch = 0;
	if (n_char && n_char < 255
			&& 0 == moves[0].ch && 255 == moves[n_char-1].ch)
	{
		// Not { [0, ch1) Union (ch2, 255] } == [ch1, ch2]
		unsigned ch1 = 0, ch2 = 255;
		unsigned idx = n_char-1;
		while (ch1 < ch2 && moves[ch1].ch == ch1) ++ch1;
		while (ch2 > ch1 && moves[idx].ch == ch2) --ch2, --idx;
		if (ch1 + 255-ch2 == n_char) {
			// now must be a neg range, such as [^a-z]
			m_neg_minch = ch1;
			m_neg_maxch = ch2;
		}
#if 0
		printf("tried neg range op: ");
		for (size_t i = 0; i < n_char; ++i) {
			auto ct = moves[i];
			printf(" %d %zd, ", ct.ch, ct.target);
		}
		printf("\n");
		printf("n_char=%zd ch1=%d ch2=%d\n", n_char, ch1, ch2);
#endif
	}
}

inline
size_t VirtualMachineDFA::Builder::matchid_slots(int inline_id_bits) const {
	if (inline_id_bits) {
		if (n_matchid) {
			if (1 == n_matchid && p_matchid[0] < 1<<inline_id_bits)
				return 0; // match_data_inline
			else // match_num__inline
				return n_matchid;
		}
	} else {
		if (n_matchid > 1)
			 return n_matchid + 1;
		else return n_matchid;
	}
	return 0;
}

inline
void VirtualMachineDFA::Builder::select_instruction(size_t state) {
	size_t ch_range = range();
	size_t id = idmap[state];
	assert(UINT32_MAX != id);

	z_slot = zstr.empty() ? 0 : (zstr.size()+1+3)/4;
	n_slot = z_slot;

	assert(n_capture <= 256);
	if (n_capture) // ncap, cap0, cap1, cap2, ...
		n_slot += (1 + n_capture + 3) / 4;

	int id_bits = 0; // inline bit field match_id bits

	switch (n_uniq) {
	case 0:
		assert(0 == n_char);
		// may be a dead state, initial_state may also be dead
		// assert(n_matchid || initial_state == state);
		op = leaf_final_state;
		i_slot = 1;
		id_bits = 23;
		break;
	case 1:
#if 0
		if (n_loop) {
			assert(n_char == n_loop);
			if (ch_range == n_char) { // fullfill, diff_val will be 0
				i_slot = 1, id_bits = 0, op = mul_loop_non_dest_fullfill;
			}
			else if (0 != m_neg_maxch) {
				assert(0 != m_neg_minch);
				i_slot = 1, id_bits = 0, op = neg_char_one_diff_fullfill;
			}
			else {
				assert(0 == m_neg_minch);
				if (ch_range <= 16)
					i_slot = 1, op = mul_loop_non_dest_small_bm;
				else            op = mul_loop_non_dest_large_bm,
					i_slot = 1 + (ch_range + 31-13) / 32;
			}
		}
		else
#endif
		{
			// the unique child id has two cases:
			//  1. has visited, so we know idmap[child]
			//  2. not visited, so idmap[child] will be (my_id + my_slots)
			if (1 == n_char) {
				size_t child = dests[0];
				uint32_t target = idmap[child];
				if (target <= id) {
					if  (id - target <= 0x8000)
						 i_slot = 1, id_bits =  0, op = one_char_one_diff;
					else i_slot = 2, id_bits = 15, op = one_char_one_dest;
				}
				else {
					if  (n_slot + matchid_slots(0) <= 0x7FFF && color.is0(child))
						 i_slot = 1, id_bits =  0, op = one_char_one_diff;
					else i_slot = 2, id_bits = 15, op = one_char_one_dest;
				}
			}
			else if (ch_range == n_char) { // fullfill
				size_t child = dests[0];
				uint32_t target = idmap[child];
				if (target <= id) {
					if  (id - target <= 128)
						 i_slot = 1, op = mul_char_one_diff_fullfill; // backward
					else i_slot = 2, op = mul_char_one_dest_fullfill;
				}
				else {
					if  (n_slot + 1 + matchid_slots(0) <= 127 && color.is0(child))
						 i_slot = 1, op = mul_char_one_diff_fullfill; // forward
					else i_slot = 2, op = mul_char_one_dest_fullfill;
				}
			}
			else if (0 != m_neg_maxch) {
				assert(0 != m_neg_minch);
				size_t child = dests[0];
				uint32_t target = idmap[child];
				if (target <= id) {
					if  (id - target <= 128)
						 i_slot = 1, op = neg_char_one_diff_fullfill; // backward
					else i_slot = 2, op = neg_char_one_dest_fullfill;
				}
				else {
					if  (n_slot + 1 + matchid_slots(0) <= 127 && color.is0(child))
						 i_slot = 1, op = neg_char_one_diff_fullfill; // forward
					else i_slot = 2, op = neg_char_one_dest_fullfill;
				}
			}
			else {
				if (ch_range <= 16)
					i_slot = 2, op = mul_char_one_dest_small_bm;
				else            op = mul_char_one_dest_large_bm,
					i_slot = 1 + (ch_range + 31 - 13) / 32 + 1;
			}
		}
		break;
	case 2:
		if (256 == n_char) {
			uint32_t target = idmap[nonloop_dest];
			if (1 == n_loop) {
				if (target <= id) { // UINT32_MAX != target || second pass
					assert(target < id);
					if  (id - target <= 0x8000)
						 i_slot = 1, id_bits =  0, op = all_diff_one_loop;
					else i_slot = 2, id_bits = 15, op = all_dest_one_loop;
				}
				else {
					if  (n_slot + 1 + matchid_slots(0) <= 0x7FFF && color.is0(nonloop_dest))
						 i_slot = 1, id_bits =  0, op = all_diff_one_loop;
					else i_slot = 2, id_bits = 15, op = all_dest_one_loop;
				}
				break;
			}
			else if (255 == n_loop) {
				if (target <= id) {
					if  (id - target <= 0x8000)
						 i_slot = 1, id_bits =  0, op = all_loop_one_diff;
					else i_slot = 2, id_bits = 15, op = all_loop_one_dest;
				}
				else {
					if  (n_slot + 1 + matchid_slots(0) <= 0x7FFF && color.is0(nonloop_dest))
						 i_slot = 1, id_bits =  0, op = all_loop_one_diff;
					else i_slot = 2, id_bits = 15, op = all_loop_one_dest;
				}
				break;
			}
		}
		else if (ch_range == n_char) { // fullfill range
			size_t nbm = (ch_range + 31 - 8) / 32;
			if (n_loop)
				i_slot = 2 + nbm, op = mul_loop_one_dest_fullfill;
			else
				i_slot = 3 + nbm, op = mul_char_two_dest_fullfill;
			break;
		}
		// else fall through:
		no_break_fallthrough;
	case 3:
		if (ch_range <= 16/2) {
			if (n_loop)
				i_slot = 0 + n_uniq, op = mul_loop_1o2_dest_small_bm;
			else
				i_slot = 1 + n_uniq, op = mul_char_2o3_dest_small_bm;
		}
		else {
			i_slot = 1 + n_uniq + (ch_range*2 + 31-12)/32;
			if (n_loop)
				op = mul_loop_1o2_dest_large_bm, i_slot--;
			else
				op = mul_char_2o3_dest_large_bm;
		}
		break;
	default:
		if (ch_range <= 16/2) {
			op = mul_char_mul_dest_small_bm;
			i_slot = 1 + 2 + n3;
		}
		else { // rank -----------v
			i_slot = 1 + 2 + n3 + 2 + (ch_range*2 + 63)/64 * 2;
			op = mul_char_mul_dest_large_bm;
		}
		if (n_uniq <= 15) {
			size_t nbm4 = (ch_range * 4 + 31)/32;
			size_t ihuge4 = 1 + nbm4 + n_uniq - (n_loop?1:0);
			if (ihuge4 <= i_slot) {
				i_slot = ihuge4;
				if (n_loop)
					 op = mul_loop_mul_dest_huge4_bm;
				else op = mul_char_mul_dest_huge4_bm;
			}
		}
		break;
	}
	size_t idx = m_iimap.find_i(m_iikey);
	if (m_iimap.end_i() != idx && n_uniq < 240) {
		auto& x = m_iimap.val(idx);
		// The accurate calculation is too complex to implement.
		// This calculation is not accurate, it is more conservative
		// to use mul_loop_index_mapped and mul_dest_index_mapped.
		//    When loop_cnt and non_loop_cnt are all not zero,
		// there is only one copy of the IndexMapEntry, but this
		// calculation regard it as two copy of IndexMapEntry.
		if (this->n_loop) {
			size_t islot2 = n_uniq;
			if (sizeof(IndexMapEntry) + 4 * x.loop_cnt * islot2
									  < 4 * x.loop_cnt * i_slot) {
				// if this branch takes, it always takes for same m_iikey
				op = mul_loop_index_mapped, i_slot = islot2;
				x.loop_cnt_real++;
			}
		}
		else {
			size_t islot2 = 1 + n_uniq;
			if (sizeof(IndexMapEntry) + 4 * x.non_loop_cnt * islot2
									  < 4 * x.non_loop_cnt * i_slot) {
				// if this branch takes, it always takes for same m_iikey
				op = mul_dest_index_mapped, i_slot = islot2;
				x.non_loop_cnt_real++;
			}
		}
	}
	n_slot += i_slot;
	n_slot += matchid_slots(id_bits);
}

inline
void
VirtualMachineDFA::Builder::emit_state(const VirtualMachineDFA& self, size_t state)
const {
#ifndef NDEBUG
	assert(idmap[state] < m_states->size());
	for (size_t i = 0; i < n_uniq; ++i) {
		assert(idmap[dests[i]] < m_states->size());
	}
#endif
	size_t ip = idmap[state];
	assert(ip < m_states->size());
	State* u = m_states->data() + ip;
	u->pzip_bit = zstr.size() ? 1 : 0;
	u->term_bit = n_matchid   ? 1 : 0;
	u->has_capt = n_capture   ? 1 : 0;

	enum {
		match_general_fmt = 0, // format: ( [-num, ] id0, id1, id2, ... )
		match_data_inline = 1, // single matchid was stored inline
		match_num__inline = 2, // num was stored inline
	}
	match_store_type = match_general_fmt;

#define SetMatchID(p, id_bits) \
	if (n_matchid) { \
		if (1 == n_matchid && p_matchid[0] < 1<<id_bits) { \
			p->match_mul = 0; \
			p->match_id = p_matchid[0]; \
			match_store_type = match_data_inline; \
		} \
		else { \
			p->match_mul = 1; \
			p->match_id = n_matchid; /*number*/ \
			match_store_type = match_num__inline; \
		} \
	}

	u->op = op;
	switch (op) {
	default:
		fprintf(stderr, "unknow OpCode = %d\n", op);
		abort();
		break;
	case leaf_final_state:
	  {
		// may be a dead state, initial_state may also be dead
		// assert(u->term_bit || initial_state == state);
		auto p = reinterpret_cast<Inst_leaf_final_state*>(u);
		SetMatchID(p, 23);
		break;
	  }
	case one_char_one_diff:
	  {
		intptr_t target = idmap[moves[0].target];
		intptr_t diff = target - intptr_t(ip);
		assert(target >= 0);
		assert(target < (intptr_t)m_states->size());
		assert(diff >= -32768 && diff <= 32767);
		u->ch = moves[0].ch;
		u->diff_val = diff;
		break;
	  }
	case all_diff_one_loop:
	  {
		intptr_t target = idmap[nonloop_dest];
		intptr_t diff = target - intptr_t(ip);
		assert(target >= 0);
		assert(target < (intptr_t)m_states->size());
		assert(diff >= -32768 && diff <= 32767);
		u->ch = theloop_char;
		u->diff_val = diff;
		break;
	  }
	case all_loop_one_diff:
	  {
		intptr_t target = idmap[nonloop_dest];
		intptr_t diff = target - intptr_t(ip);
		assert(target >= 0);
		assert(target < (intptr_t)m_states->size());
		assert(diff >= -32768 && diff <= 32767);
		u->ch = nonloop_char;
		u->diff_val = diff;
		break;
	  }
	case one_char_one_dest:
	  {
		assert(0 == n_loop);
		assert(1 == n_char);
		auto p = reinterpret_cast<Inst_one_char_one_dest*>(u);
		p->ch     = moves[0].ch;
		p->target = idmap[moves[0].target];
		SetMatchID(p, 15);
		break;
	  }
	case mul_char_one_diff_fullfill:
	  {
		auto p = (Inst_mul_char_one_diff_fullfill*)(u);
		intptr_t target = idmap[moves[0].target];
		intptr_t diff = target - intptr_t(ip);
		assert(target >= 0);
		assert(target < (intptr_t)m_states->size());
		assert(diff >= -128);
		assert(diff <= +127);
		p->minch = this->minch();
		p->maxch = this->maxch();
		p->diff_val = diff;
		break;
	  }
	case mul_char_one_dest_fullfill:
	  {
		auto p = (Inst_mul_char_one_dest_fullfill*)(u);
		p->minch = this->minch();
		p->maxch = this->maxch();
		p->target = idmap[moves[0].target];
		break;
	  }
	case neg_char_one_diff_fullfill:
	  {
		auto p = (Inst_neg_char_one_diff_fullfill*)(u);
		intptr_t target = idmap[moves[0].target];
		intptr_t diff = target - intptr_t(ip);
		assert(target >= 0);
		assert(target < (intptr_t)m_states->size());
		assert(diff >= -128 && diff <= 127);
		p->minch = this->m_neg_minch;
		p->maxch = this->m_neg_maxch;
		p->diff_val = diff;
		break;
	  }
	case neg_char_one_dest_fullfill:
	  {
		auto p = (Inst_neg_char_one_dest_fullfill*)(u);
		p->minch = this->m_neg_minch;
		p->maxch = this->m_neg_maxch;
		p->target = idmap[moves[0].target];
		break;
	  }
	case mul_char_one_dest_small_bm:
	  {
		// one_char_one_loop should be very rare: "a*$"
		// use this as a genernal case
		auto p = reinterpret_cast<Inst_mul_char_one_dest_small_bm*>(u);
		auto ch0 = this->minch();
		p->minch = ch0;
		for(size_t j = 0; j < n_char; ++j) {
			size_t k = moves[j].ch - ch0;
			p->bits1 |= one << k;
		}
		p->target = idmap[moves[0].target];
		break;
	  }
	case mul_char_one_dest_large_bm:
	  {
		auto p = reinterpret_cast<Inst_mul_char_one_dest_large_bm*>(u);
		auto nbm = (range() + 31 - 13) / 32;
		auto ch0 = minch();
		p->nbm = nbm - 1;
		p->minch = ch0;
		for(size_t j = 0; j < n_char; ++j) {
			intptr_t k = moves[j].ch - ch0 + (32 - 13);
			p->tarbits[k/32-1] |= one << (k % 32);
		}
		p->tarbits[nbm] = idmap[moves[0].target];
		break;
	  }
	case mul_char_two_dest_fullfill:
	  {
		auto p = reinterpret_cast<Inst_mul_char_two_dest_fullfill*>(u);
		auto nbm = (range() + 31 - 8) / 32;
		auto ch0 = minch();
		p->minch = ch0;
		p->maxch = this->maxch();
		for(size_t j = 0; j < n_char; ++j) {
			intptr_t k = moves[j].ch - ch0 + (32 - 8);
			if (moves[j].target == t2)
				p->tarbits[k/32-1] |= one << (k % 32);
		}
		p->tarbits[nbm+0] = idmap[t1];
		p->tarbits[nbm+1] = idmap[t2];
		break;
	  }
	case mul_loop_one_dest_fullfill:
	  {
		auto p = reinterpret_cast<Inst_mul_loop_one_dest_fullfill*>(u);
		auto nbm = (range() + 31 - 8) / 32;
		auto ch0 = minch();
		p->minch = ch0;
		p->maxch = this->maxch();
		for(size_t j = 0; j < n_char; ++j) {
			intptr_t k = moves[j].ch - ch0 + (32 - 8);
			if (moves[j].target != state)
				p->tarbits[k/32-1] |= one << (k % 32);
		}
		p->tarbits[nbm] = idmap[nonloop_dest];
		break;
	  }
	case mul_char_2o3_dest_small_bm:
	  {
		unsigned ch0 = minch();
		auto p = reinterpret_cast<Inst_mul_char_2o3_dest_small_bm*>(u);
		p->minch = ch0;
		for(size_t j = 0; j < n_char; ++j) {
			unsigned t = moves[j].target;
			intptr_t i = moves[j].ch - ch0;
			unsigned b = 1 + lower_bound_0(dests.p, n_uniq, t);
			p->bits1 |= b << (2*i);
		}
		for (size_t j = 0; j < n_uniq; ++j)
			p->targets[j] = idmap[dests[j]];
		break;
	  }
	case mul_char_2o3_dest_large_bm:
	  {
		unsigned ch0 = minch();
		auto p = reinterpret_cast<Inst_mul_char_2o3_dest_large_bm*>(u);
		auto nbm = (2*range() + 31-12) / 32;
		p->minch = ch0;
		p->nbm = nbm - 1;
		for(size_t j = 0; j < n_char; ++j) {
			unsigned t = moves[j].target;
			intptr_t i = 2*(moves[j].ch - ch0) + 32-12;
			unsigned b = 1 + lower_bound_0(dests.p, n_uniq, t);
			p->tarbits[i/32-1] |= b << (i % 32);
		}
		for (size_t j = 0; j < n_uniq; ++j)
			p->tarbits[nbm + j] = idmap[dests[j]];
		break;
	  }
	case mul_char_mul_dest_small_bm:
	  {
		auto p = (Inst_mul_char_mul_dest_small_bm*)(u);
		unsigned ch0 = this->minch();
		unsigned bits1 = 0;
		for (size_t j = 0; j < n_char; ++j) {
			unsigned t = moves[j].target;
			unsigned b = t==t1 ? 1 : t==t2 ? 2 : 3;
			bits1 |= b << 2*(moves[j].ch - ch0);
		}
		p->minch = ch0;
		p->bits1 = bits1;
		p->target1 = idmap[t1];
		p->target2 = idmap[t2];
		size_t i = 0;
		for (size_t j = 0; j < n_char; ++j) {
			unsigned t = moves[j].target;
			if (t != t1 && t != t2)
				p->targets[i++] = idmap[t];
		}
		break;
	  }
	case mul_char_mul_dest_large_bm:
	  {
		auto p = (Inst_mul_char_mul_dest_large_bm*)(u);
		unsigned ch0 = this->minch();
		unsigned nbm = (2*this->range() + 63)/64;
		for (size_t j = 0; j < n_char; ++j) {
			intptr_t i = (moves[j].ch - ch0) * 2;
			unsigned t = moves[j].target;
			uint32_t b = t==t1 ? 1 : t==t2 ? 2 : 3;
			p->ubits.bm32[i/32] |= b << i%32;
		}
		{
			size_t j = 0, r = 0;
			for (; j < nbm; ++j) {
				p->rank[j] = r;
				uint64_t b64 = p->ubits.bm64[j];
				r += fast_popcount64(b64 & (b64 >> 1) & BitMask64);
			}
			assert(r <= 256);
			assert(r == n3);
			p->r_sum = r - 1;
		}
		p->minch = ch0;
		p->maxch = this->maxch();
		p->target1 = idmap[t1];
		p->target2 = idmap[t2];
		u32* targets = p->ubits.bm32 + nbm * 2;
		for (size_t i = 0, j = 0; j < n_char; ++j) {
			unsigned t = moves[j].target;
			if (t != t1 && t != t2)
				targets[i++] = idmap[t];
		}
		break;
	  }
	case mul_loop_1o2_dest_small_bm:
	  {
		assert(n_uniq <= 3);
		size_t targets[3];
		std::copy_n(dests.p, n_uniq, targets);
		std::remove(targets, targets+n_uniq, size_t(state));
		targets[n_uniq-1] = UINT32_MAX;
		auto p = (Inst_mul_loop_1o2_dest_small_bm*)(u);
		unsigned ch0 = minch();
		p->minch = ch0;
		for(size_t j = 0; j < n_char; ++j) {
			auto c = moves[j].ch;
			auto t = moves[j].target;
			auto b = t==state ? 1 : 2 + lower_bound_0(targets, n_uniq-1, t);
			p->bits1 |= b << 2*(c-ch0);
		}
		for (size_t j = 0; j < n_uniq-1; ++j)
			p->targets[j] = idmap[targets[j]];
		break;
	  }
	case mul_loop_1o2_dest_large_bm:
	  {
		assert(n_uniq <= 3);
		size_t targets[3];
		std::copy_n(dests.p, n_uniq, targets);
		std::remove(targets, targets+n_uniq, size_t(state));
		targets[n_uniq-1] = UINT32_MAX;
		auto p = (Inst_mul_loop_1o2_dest_large_bm*)(u);
		auto nbm = (range() * 2 + 31 - 12) / 32;
		unsigned ch0 = minch();
		p->nbm = nbm - 1;
		p->minch = ch0;
		for(size_t j = 0; j < n_char; ++j) {
			auto k = intptr_t(moves[j].ch - ch0) * 2 + 32 - 12;
			auto t = moves[j].target;
			auto b = t==state ? 1 : 2 + lower_bound_0(targets, n_uniq-1, t);
			p->tarbits[k/32-1] |= b << (k % 32);
		}
		for (size_t j = 0; j < n_uniq-1; ++j)
			p->tarbits[nbm + j] = idmap[targets[j]];
		break;
	  }
	case mul_loop_mul_dest_huge4_bm:
	  // when loop, 'state' is not in freq_map, then
	  assert(freq_map.end_i()+1 == n_uniq);
	  assert(freq_map.find_i(state) == n_uniq-1);
	  // fall through
	  no_break_fallthrough;
	case mul_char_mul_dest_huge4_bm:
	  {
		assert(n_uniq >=  4);
		assert(n_uniq <= 15);
		size_t ch0 = moves[0].ch;
		size_t nbm = (range() * 4 + 31) / 32;
		auto p = (Inst_mul_char_mul_dest_huge4_bm*)(u);
		p->minch = this->minch();
		p->maxch = this->maxch();
		p->nUniq = freq_map.end_i();
		for(size_t j = 0; j < n_char; ++j) {
			size_t k = (moves[j].ch - ch0) * 4;
			size_t b = n_uniq - freq_map.find_i(moves[j].target);
			assert(b <= n_uniq);
			p->tarbits[k/32] |= b << (k%32);
		}
		for(size_t j = 0, n = freq_map.end_i(); j < n; ++j) {
			assert(idmap[freq_map.key(n-j-1)] < m_states->size());
			p->tarbits[nbm + j] = idmap[freq_map.key(n-j-1)];
		}
		break;
	  }
#if 0
	case mul_loop_non_dest_fullfill:
	  {
		auto p = reinterpret_cast<Inst_mul_loop_non_dest_fullfill*>(u);
		p->minch = this->minch();
		p->maxch = this->maxch();
		break;
	  }
	case mul_loop_non_dest_small_bm:
	  {
		unsigned ch0 = moves[0].ch;
		unsigned bits = 0;
		for(size_t j = 0; j < n_char; ++j) {
			size_t k = moves[j].ch - ch0;
			bits |= one << k;
		}
		auto p = reinterpret_cast<Inst_mul_loop_non_dest_small_bm*>(u);
		p->minch = ch0;
		p->bits1 = bits;
		break;
	  }
	case mul_loop_non_dest_large_bm:
	  {
		unsigned ch0 = moves[0].ch;
		auto p = reinterpret_cast<Inst_mul_loop_non_dest_large_bm*>(u);
		p->minch = ch0;
		p->nbm = (this->range() + 31 - 13) / 32 - 1;
		for(size_t j = 0; j < n_char; ++j) {
			intptr_t k = moves[j].ch - ch0 + (32 - 13);
			p->bitvec[k/32-1] |= one << k%32;
		}
		break;
	  }
#endif
	case all_dest_one_loop:
	  {
		assert(256 == n_char);
		auto p = reinterpret_cast<Inst_one_char_one_dest*>(u);
		p->ch     = theloop_char;
		p->target = idmap[nonloop_dest];
		SetMatchID(p, 15);
		break;
	  }
	case all_loop_one_dest:
	  {
		assert(256 == n_char);
		auto p = reinterpret_cast<Inst_one_char_one_dest*>(u);
		p->ch     = nonloop_char;
		p->target = idmap[nonloop_dest];
		SetMatchID(p, 15);
		break;
	  }
	case mul_dest_index_mapped:
	  {
		auto p = reinterpret_cast<Inst_mul_dest_index_mapped*>(u);
		p->map_id = m_iimap.find_i(m_iikey);
		assert(p->map_id < m_iimap.end_i());
		for(size_t k = 0; k < n_uniq; ++k) {
			size_t t = dests.p[k];
			size_t ch_id = m_serialset.find_i(t);
			p->targets[ch_id] = idmap[t];
		}
		break;
	  }
	case mul_loop_index_mapped:
	  {
		auto p = reinterpret_cast<Inst_mul_dest_index_mapped*>(u);
		p->map_id = m_iimap.find_i(m_iikey);
		assert(p->map_id < m_iimap.end_i());
		for(size_t k = 0; k < n_uniq; ++k) {
			size_t t = dests.p[k];
			if (t != state) {
				size_t ch_id = m_serialset.find_i(t);
				assert(0 != ch_id);
				p->targets[ch_id - 1] = idmap[t];
			}
		}
		break;
	  }
	}
#undef SetMatchID

#if defined(NDEBUG)
	size_t computed_n_slots = this->i_slot;
#else
	size_t computed_i_slots = self.instruction_slots(u);
	size_t computed_n_slots = computed_i_slots;
	assert(computed_i_slots == this->i_slot);
#endif
	if (z_slot) {
		byte_t* zptr = (byte_t*)(u + i_slot);
		size_t  zlen = zstr.size();
		zptr[0] = zlen;
		memcpy(zptr+1, zstr.data(), zlen);
		computed_n_slots += (zlen+1+3)/4;
	}

	byte_t* capture_ptr = NULL;
	if (n_matchid && match_data_inline != match_store_type) {
		s32* pm = (s32*)(u + i_slot + z_slot);
		if (match_general_fmt == match_store_type && n_matchid > 1) {
			*pm++ = -s32(n_matchid);
			computed_n_slots += 1;
		}
		computed_n_slots += n_matchid;
		for (size_t i = 0; i < n_matchid; ++i) pm[i] = p_matchid[i];
		capture_ptr = (byte_t*)(pm + n_matchid);
	}
	else {
		capture_ptr = (byte_t*)(u + i_slot + z_slot);
	}
	if (n_capture) {
		assert(n_capture <= 256);
		computed_n_slots += (n_capture+1+3)/4;
		capture_ptr[0] = n_capture-1;
		for (size_t i = 0; i < n_capture; ++i) {
			assert(p_capture[i] >= 2);
			assert(p_capture[i] <= 257);
			capture_ptr[i+1] = p_capture[i] - 2;
		}
	}
	assert(computed_n_slots == this->n_slot);
	if (computed_n_slots != this->n_slot)
		THROW_STD(runtime_error, "internal error");
}

template<class Au, class AuID>
void VirtualMachineDFA::build_from(const Au& au
		, const AuID* src_roots, size_t num_roots
		, state_id_t* dst_roots) {
	Builder builder(au, &states);
	builder.build(*this, au, src_roots, num_roots, dst_roots);
	m_atom_dfa_num = au.m_atom_dfa_num;
	m_dfa_cluster_num = au.m_dfa_cluster_num;
	m_gnode_states = builder.m_gnode_states;
}

} // namespace terark
