#include "louds_dfa.hpp"
#include "dfa_mmap_header.hpp"
#include "tmplinst.hpp"
#include "fast_search_byte.hpp"

namespace terark {

template<class RankSelect>
GenLoudsDFA<RankSelect>::GenLoudsDFA() {
	m_label_data = NULL;
	m_zpath_data = NULL;
	m_zpath_trie = NULL;
	m_is_dag = true;
	m_dyn_sigma = 256;
	m_zpNestLevel = 0;
}

template<class RankSelect>
GenLoudsDFA<RankSelect>::GenLoudsDFA(const GenLoudsDFA& y)
  : MatchingDFA(y)
  , m_louds(y.m_louds)
  , m_is_pzip(y.m_is_pzip)
  , m_is_term(y.m_is_term)
  , m_cross_edge_dst(y.m_cross_edge_dst)
  , m_is_cross_edge(y.m_is_cross_edge)
  , m_zpath_index(y.m_zpath_index)
  , m_zpNestLevel(y.m_zpNestLevel)
{
	size_t label_size = y.total_states() + mm_cmpestri_extra;
	m_dyn_sigma = 256;
	m_is_dag = true;
	m_zpath_trie = y.m_zpath_trie
				 ? new NestLoudsTrieTpl<RankSelect>(*y.m_zpath_trie)
				 : NULL;
	m_label_data = AutoFree<byte_t>(
		label_size + (m_zpath_trie ? 0 : y.m_total_zpath_len+1),
		y.m_label_data).release();
	if (m_zpath_trie) {
		assert(NULL == y.m_zpath_data);
		m_zpath_data = NULL;
	}
	else {
		assert(NULL != y.m_zpath_data);
		m_zpath_data = m_label_data + label_size;
	}
	this->mmap_base = NULL;
	this->m_mmap_type = 0;
}

#if defined(_MSC_VER)
// warning C4291 : 'void *terark::CacheAlignedNewDelete::operator new(size_t,void *)' :
// no matching operator delete found; memory will not be freed if initialization throws an exception
// -- because we defined placement new in CacheAlignedNewDelete, this should be a msvc bug
#pragma warning(disable: 4291)
#endif

template<class RankSelect>
GenLoudsDFA<RankSelect>&
GenLoudsDFA<RankSelect>::operator=(const GenLoudsDFA& y) {
	if (&y != this) {
		this->~GenLoudsDFA();
		new(this)GenLoudsDFA(y);
	}
	return *this;
}

template<class RankSelect>
GenLoudsDFA<RankSelect>::~GenLoudsDFA() {
	if (this->mmap_base) {
		m_louds.risk_release_ownership();
		m_is_pzip.risk_release_ownership();
		m_is_term.risk_release_ownership();
		m_cross_edge_dst.risk_release_ownership();
		m_is_cross_edge.risk_release_ownership();
		m_zpath_index.risk_release_ownership();
		m_zpath_id.risk_release_ownership();
		m_cache.risk_release_ownership();
		if (m_zpath_trie)
			m_zpath_trie->risk_release_ownership();
	}
	else {
		if (m_label_data) free(m_label_data);
	}
	delete m_zpath_trie;
}

template<class RankSelect>
void GenLoudsDFA<RankSelect>::swap(GenLoudsDFA& y) {
	m_louds.swap(y.m_louds);
	m_is_pzip.swap(y.m_is_pzip);
	m_is_term.swap(y.m_is_term);
	m_is_cross_edge.swap(y.m_is_cross_edge);
	m_cross_edge_dst.swap(y.m_cross_edge_dst);
	m_zpath_index.swap(y.m_zpath_index);
	std::swap(m_label_data, y.m_label_data);
	std::swap(m_zpath_data, y.m_zpath_data);
	std::swap(m_cache, y.m_cache);
	std::swap(m_zpath_trie, y.m_zpath_trie);
}

template<class RankSelect>
bool GenLoudsDFA<RankSelect>::has_freelist() const {
	return true;
}

template<class RankSelect>
fstring GenLoudsDFA<RankSelect>::get_zpath_data(size_t s, MatchContext* ctx) const {
	assert(NULL != ctx);
	assert(s < total_states());
	assert(m_is_pzip.size() > 0);
	assert(m_is_pzip[s]);
	size_t znth = m_is_pzip.rank1(s);
	if (m_zpath_trie) {
		size_t link_id = m_zpath_id[znth];
		m_zpath_trie->restore_string(link_id, &ctx->zbuf);
		return fstring(ctx->zbuf.data(), ctx->zbuf.size());
	}
	else {
		size_t bitpos = m_zpath_index.select0(znth);
		assert(m_zpath_index.rank0(bitpos) == znth);
		assert(m_zpath_index.is0(bitpos));
		bitpos++;
		size_t zbeg = bitpos + znth;
		size_t zlen = m_zpath_index.one_seq_len(bitpos) + 2;
		assert(zbeg + zlen <= m_total_zpath_len+1);
		return fstring((const char*)&m_zpath_data[zbeg], zlen);
	}
}

template<class RankSelect>
size_t GenLoudsDFA<RankSelect>::mem_size() const {
	return m_louds.mem_size()
	     + m_is_pzip.mem_size()
		 + m_is_term.mem_size()
	     + m_is_cross_edge.mem_size()
	     + m_cross_edge_dst.mem_size()
		 + m_cache.mem_size()
	     + total_states() // m_label_data
		 + (m_zpath_trie
		    ? m_zpath_trie->mem_size() + m_zpath_id.mem_size()
			: m_zpath_index.mem_size() + m_total_zpath_len+1)
		 ;
}

template<class RankSelect>
size_t GenLoudsDFA<RankSelect>::state_move(size_t curr, auchar_t ch)
const {
	assert(!m_is_cross_edge[curr]);
	size_t bitpos;
	if (curr < m_cache.size()) {
		bitpos = m_cache[curr];
	} else {
		bitpos = m_louds.select0(curr);
		assert(m_louds.rank0(bitpos) == curr);
		assert(m_louds.is0(bitpos));
		bitpos++;
	}
	size_t lcount = m_louds.one_seq_len(bitpos);
	size_t child0 = bitpos - curr;
	return state_move_fast(curr, ch, lcount, child0);
}

template<class RankSelect>
size_t GenLoudsDFA<RankSelect>::
state_move_fast(size_t curr, auchar_t ch, size_t lcount, size_t child0)
const {
	if (lcount < MIN_FAST_SEARCH_BYTES) { // labels saved as byte array
		size_t ch_idx = fast_search_byte(m_label_data + child0, lcount, ch);
		if (ch_idx < lcount) {
			size_t child = child0 + ch_idx;
			if (m_is_cross_edge[child]) {
				size_t cross_idx = m_is_cross_edge.rank1(child);
				child = m_cross_edge_dst[cross_idx];
			}
			return child;
		} else
			return nil_state;
	}
   	else { // labels saved as popcnt_sum + bitmap256
		const byte_t* mylabels = m_label_data + child0;
		uint32_t w = unaligned_load<uint32_t>(mylabels + POPCNT_BYTES, ch/32);
		if (!(w & (uint32_t(1) << ch%32))) {
			return nil_state;
		}
		size_t ch_idx = mylabels[ch/32] + fast_popcount_trail(w, ch%32);
		assert(ch_idx < lcount);
		size_t child = child0 + ch_idx;
		if (m_is_cross_edge[child]) {
			size_t cross_idx = m_is_cross_edge.rank1(child);
			child = m_cross_edge_dst[cross_idx];
		}
		return child;
	}
}

template<class RankSelect>
size_t GenLoudsDFA<RankSelect>::state_move_slow(size_t curr, auchar_t ch, StateMoveContext& ctx) const {
	assert(!m_is_cross_edge[curr]);
	size_t bitpos;
	if (curr < m_cache.size()) {
		bitpos = m_cache[curr];
		assert(bitpos < m_louds.size());
	} else {
		bitpos = m_louds.select0(curr);
		assert(bitpos + 1 < m_louds.size());
		assert(m_louds.rank0(bitpos) == curr);
		assert(m_louds.is0(bitpos));
		bitpos++;
	}
	size_t lcount = m_louds.one_seq_len(bitpos);
	assert(bitpos + lcount < m_louds.size());
	if (terark_unlikely(0 == lcount)) {
		ctx.n_children = 0;
		ctx.child0 = size_t(-1);
		return nil_state;
	}
	else {
		size_t child0 = bitpos - curr;
		size_t child  = state_move_fast(curr, ch, lcount, child0);
		ctx.n_children = lcount;
		ctx.child0 = child0;
		return child;
	}
}

template<class RankSelect>
size_t GenLoudsDFA<RankSelect>::v_num_children(size_t parent) const {
	assert(!m_is_cross_edge[parent]);
	size_t bitpos = m_louds.select0(parent);
	assert(m_louds.rank0(bitpos) == parent);
	assert(m_louds.is0(bitpos));
	bitpos++;
	return m_louds.one_seq_len(bitpos);
}

template<class RankSelect>
bool GenLoudsDFA<RankSelect>::v_has_children(size_t parent) const {
	assert(!m_is_cross_edge[parent]);
	assert(parent < total_states());
	size_t bitpos = m_louds.select0(parent);
	assert(m_louds.rank0(bitpos) == parent);
	assert(m_louds.is0(bitpos));
	bitpos++;
	return m_louds.is1(bitpos);
}

template<class RankSelect>
size_t GenLoudsDFA<RankSelect>::v_gnode_states() const {
	return total_states() - m_cross_edge_dst.size();
}

template<class RankSelect>
size_t GenLoudsDFA<RankSelect>::zp_nest_level() const {
	return m_zpNestLevel;
}

template<class RankSelect>
void GenLoudsDFA<RankSelect>::finish_load_mmap(const DFA_MmapHeader* base) {
	byte_t* bbase = (byte_t*)base;
	if (base->transition_num >= 0x7FFFFFFF) {
		THROW_STD(out_of_range, "transition_num=%lld",
			   	(long long)base->transition_num);
	}
	assert(base->total_states - 1 == base->transition_num);
	if (base->louds_dfa_num_zpath_trie) {
		m_zpath_trie = new NestLoudsTrieTpl<RankSelect>();
	}
	auto* blocks = base->blocks;
	size_t edges = base->transition_num;
	size_t cross_edge_num = size_t(base->numFreeStates);
	m_louds.risk_mmap_from(bbase + blocks[0].offset, blocks[0].length);
	assert(m_louds.size() == 2 * edges + 2);
	m_zpath_states = size_t(base->zpath_states);
	if (m_zpath_states) {
		m_is_pzip.risk_mmap_from(bbase + blocks[1].offset, blocks[1].length);
		assert(m_is_pzip.size() == edges + 1);
	}
	m_is_term      .risk_mmap_from(bbase + blocks[2].offset, blocks[2].length);
	m_cross_edge_dst.risk_set_data(bbase + blocks[3].offset, cross_edge_num
		, size_t(base->louds_dfa_min_cross_dst)
		, base->louds_dfa_cross_dst_uintbits
		);
	m_is_cross_edge.risk_mmap_from(bbase + blocks[4].offset, blocks[4].length);
	assert(m_is_cross_edge.size() == edges + 1);
	assert(m_cross_edge_dst.mem_size() <= blocks[3].length);

	m_label_data = bbase + blocks[5].offset;

	if (m_zpath_trie) {
		m_zpath_trie->load_mmap(bbase + blocks[6].offset, blocks[6].length);
	}
	else {
		m_zpath_data = m_label_data + edges + mm_cmpestri_extra + 1;
		if (m_zpath_states) {
			m_zpath_index.risk_mmap_from(bbase + blocks[6].offset, blocks[6].length);
			assert(m_zpath_index.size() == base->zpath_length - m_zpath_states + 1);
		}
		assert(base->zpath_length+1 <= blocks[5].length - (edges + 1 + mm_cmpestri_extra));
	}
	m_total_zpath_len = base->zpath_length;

	if (base->louds_dfa_cache_ptrbit) {
		m_cache.risk_set_data(bbase + blocks[7].offset
			, base->louds_dfa_cache_states
			, size_t(0) // UintVector::min_val()
			, base->louds_dfa_cache_ptrbit);
		assert(m_cache.mem_size() <= blocks[7].length);
	}
	if (m_zpath_trie) {
		m_zpath_id.risk_set_data(bbase + blocks[8].offset
			, m_is_pzip.max_rank1()
			, size_t(base->louds_dfa_min_zpath_id)
			, terark_bsr_u32(uint32_t(
				m_zpath_trie->total_states()-1 - base->louds_dfa_min_zpath_id)) + 1
			);
		assert(m_zpath_id.mem_size() <= blocks[8].length);
		m_zpNestLevel = m_zpath_trie->nest_level();
	}
	assert(m_is_cross_edge.size() == size_t(base->total_states));
}

template<class RankSelect>
long GenLoudsDFA<RankSelect>::prepare_save_mmap(DFA_MmapHeader* base, const void** dataPtrs)
const {
	base->state_size   = 1;
	base->transition_num = total_states() - 1;
	base->num_blocks   = 8;

	base->blocks[0].offset = sizeof(DFA_MmapHeader);
	base->blocks[0].length = m_louds.mem_size();
	dataPtrs[0] = m_louds.data();

	base->blocks[1].offset = align_to_64(base->blocks[0].endpos());
	base->blocks[1].length = m_is_pzip.mem_size();
	dataPtrs[1] = m_is_pzip.data();

	base->blocks[2].offset = align_to_64(base->blocks[1].endpos());
	base->blocks[2].length = m_is_term.mem_size();
	dataPtrs[2] = m_is_term.data();

	base->blocks[3].offset = align_to_64(base->blocks[2].endpos());
	base->blocks[3].length = m_cross_edge_dst.mem_size();
	base->louds_dfa_min_cross_dst = m_cross_edge_dst.min_val();
	base->louds_dfa_cross_dst_uintbits = m_cross_edge_dst.uintbits();
	dataPtrs[3] = m_cross_edge_dst.data();

	base->blocks[4].offset = align_to_64(base->blocks[3].endpos());
	base->blocks[4].length = m_is_cross_edge.mem_size();
	dataPtrs[4] = m_is_cross_edge.data();

	size_t label_size = total_states() + mm_cmpestri_extra;
	base->blocks[5].offset = align_to_64(base->blocks[4].endpos());
	base->blocks[5].length = label_size
						+ (m_zpath_trie ? 0 : m_total_zpath_len+1);
	dataPtrs[5] = m_label_data;

	base->blocks[6].offset = align_to_64(base->blocks[5].endpos());
	long need_free_mask = 0;
	if (m_zpath_trie) {
		need_free_mask |= long(1) << 6;
		assert(m_zpath_index.size() == 0);
		size_t trie_bytes = 0;
		const void* trie_mem = m_zpath_trie->save_mmap(&trie_bytes);
		// save m_zpath_trie
		base->blocks[6].length = trie_bytes;
		dataPtrs[6] = trie_mem;
		base->louds_dfa_num_zpath_trie = m_zpath_trie->nest_level();
	}
	else {
		base->louds_dfa_num_zpath_trie = 0;
		base->blocks[6].length = m_zpath_index.mem_size();
		dataPtrs[6] = m_zpath_index.data();
	}

	base->blocks[7].offset = align_to_64(base->blocks[6].endpos());
	base->blocks[7].length = m_cache.mem_size();
	dataPtrs[7] = m_cache.data();
	assert(m_cache.min_val() == 0);
	base->louds_dfa_cache_ptrbit = m_cache.uintbits();
	base->louds_dfa_cache_states = m_cache.size();

	if (m_zpath_trie) {
		base->blocks[8].offset = align_to_64(base->blocks[7].endpos());
		base->blocks[8].length = m_zpath_id.mem_size();
		dataPtrs[8] = m_zpath_id.data();
		base->num_blocks++;
		base->louds_dfa_min_zpath_id = m_zpath_id.min_val();
	}
	base->numFreeStates = m_cross_edge_dst.size();

	return need_free_mask;
}

template class GenLoudsDFA<rank_select_se>;
template class GenLoudsDFA<rank_select_il>;
template class GenLoudsDFA<rank_select_se_512>;

TMPL_INST_DFA_CLASS(LoudsDFA_SE)
TMPL_INST_DFA_CLASS(LoudsDFA_IL)
TMPL_INST_DFA_CLASS(LoudsDFA_SE_512)

} // namespace terark

