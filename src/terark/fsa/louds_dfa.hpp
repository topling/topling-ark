#pragma once

#include <terark/fsa/dfa_algo.hpp>
#include <terark/fsa/fsa.hpp>
#include <terark/fsa/nest_louds_trie.hpp>
#include <terark/fsa/graph_walker.hpp>
#include <terark/bitmap.hpp>
#include <terark/int_vector.hpp>
#include <terark/succinct/rank_select_se_256.hpp>
#include <terark/succinct/rank_select_il_256.hpp>
#include <terark/succinct/rank_select_se_512.hpp>
#include <terark/succinct/rank_select_mixed_il_256.hpp>
#include <terark/succinct/rank_select_mixed_xl_256.hpp>
#include <terark/succinct/rank_select_mixed_se_512.hpp>

namespace terark {

template<class RankSelect>
class TERARK_DLL_EXPORT GenLoudsDFA : public MatchingDFA {
protected:
	static const size_t mm_cmpestri_extra = 16-1;
	RankSelect    m_louds;
	RankSelect    m_is_pzip;  // m_pzip[node_id]
	febitvec      m_is_term;  // m_term[node_id]
	UintVector    m_cross_edge_dst;
	RankSelect    m_is_cross_edge; // bitpos is tree node id
	byte_t*       m_label_data;
	byte_t*       m_zpath_data;  // for easy access, =(label + labelsize)
	RankSelect    m_zpath_index; // variable length index
	UintVector    m_cache; // cache for m_louds.select0(state_id)
	UintVector    m_zpath_id; // of m_zpath_trie
	NestLoudsTrieTpl<RankSelect>* m_zpath_trie;
	size_t m_zpNestLevel;
/*
	struct LabelBitmapIndex {
		uint08_t rank_sum[8];
		uint32_t bits_u32[8];
	};
	BOOST_STATIC_ASSERT(sizeof(LabelBitmapIndex) == 40);
*/
	static const size_t POPCNT_BYTES =  8;
	static const size_t BITMAP_BYTES = 32;
	static const size_t MIN_FAST_SEARCH_BYTES = 40;//BITMAP_BYTES+

public:
	typedef size_t state_id_t;
	static const size_t nil_state = size_t(-1);
	static const size_t max_state = size_t(-1) - 1;
	enum { sigma = 256 };

	GenLoudsDFA();
	GenLoudsDFA(const GenLoudsDFA&);
	GenLoudsDFA& operator=(const GenLoudsDFA&);
	~GenLoudsDFA();

//	void clear();
	void swap(GenLoudsDFA&);

//	void erase_all() { clear(); }

	size_t total_states() const { return m_is_term.size(); }

	bool has_freelist() const override;
	// just pseudo free state
	// a free state will never be exposed by GenLoudsDFA api
	// so it seems like a 'free' state
	bool   is_free(size_t s) const { return m_is_cross_edge[s]; }

	bool   is_pzip(size_t s) const { return m_is_pzip.size() && m_is_pzip[s]; }
	bool   is_term(size_t s) const { return m_is_term[s]; }
	bool   v_has_children(size_t) const override final;
	size_t v_num_children(size_t s) const final;
	size_t v_gnode_states() const override final;
	size_t zp_nest_level() const override final;

	fstring get_zpath_data(size_t, MatchContext*) const;
	size_t mem_size() const override final;

	struct StateMoveContext {
		size_t n_children;
		size_t child0;
	};
	size_t state_move(size_t, auchar_t ch) const;
	size_t state_move_slow(size_t, auchar_t ch, StateMoveContext& ctx) const;
	size_t state_move_fast(size_t, auchar_t ch, size_t n_children, size_t child0) const;
	size_t state_move_fast(size_t parent, auchar_t ch, const StateMoveContext& ctx) const {
		return state_move_fast(parent, ch, ctx.n_children, ctx.child0); }

	template<class Au>
	void build_from(const Au&, const NestLoudsTrieConfig& conf);

	template<class DFA>
	static size_t calc_cross_edge_num(const DFA& dfa);

	template<class OP>
	void for_each_move(size_t parent, OP op) const {
		assert(parent < total_states());
		assert(!m_is_cross_edge[parent]);
		size_t bitpos = m_louds.select0(parent);
		assert(m_louds.rank0(bitpos) == parent);
		assert(m_louds.is0(bitpos));
		bitpos++;
		size_t child0 = bitpos - parent;
		size_t lcount = m_louds.one_seq_len(bitpos);
		assert(lcount <= sigma);
		assert(child0 + lcount <= total_states());
		const byte_t* mylabel = m_label_data + child0;
		if (lcount < MIN_FAST_SEARCH_BYTES) {
			for(size_t i = 0; i < lcount; ++i) {
				size_t child = child0 + i;
				if (m_is_cross_edge[child]) {
					size_t cross_idx = m_is_cross_edge.rank1(child);
					child = m_cross_edge_dst[cross_idx];
				}
				op(child, mylabel[i]);
			}
		}
	   	else {
			mylabel += POPCNT_BYTES; // point to bitmap
			size_t j = 0;
			for(size_t i = 0; i < sigma/32; ++i) {
				uint32_t bm = unaligned_load<uint32_t>(mylabel, i);
				unsigned  ch = i * 32;
				for (; bm; ++ch, ++j) {
					int ctz = fast_ctz(bm);
					size_t child = child0 + j;
					if (m_is_cross_edge[child]) {
						size_t cross_idx = m_is_cross_edge.rank1(child);
						child = m_cross_edge_dst[cross_idx];
					}
					ch += ctz;
					op(child, ch);
					bm >>= ctz;
					bm >>= 1;
				}
			}
			assert(j == lcount);
		}
	}
	template<class OP>
	void for_each_dest(size_t parent, OP op) const {
		assert(parent < total_states());
		assert(!m_is_cross_edge[parent]);
		size_t bitpos = m_louds.select0(parent);
		assert(m_louds.rank0(bitpos) == parent);
		assert(m_louds.is0(bitpos));
		bitpos++;
		size_t child0 = bitpos - parent;
		size_t lcount = m_louds.one_seq_len(bitpos);
		assert(lcount <= sigma);
		assert(child0 + lcount <= total_states());
		for(size_t i = 0; i < lcount; ++i) {
			size_t child = child0 + i;
			if (m_is_cross_edge[child]) {
				size_t cross_idx = m_is_cross_edge.rank1(child);
				child = m_cross_edge_dst[cross_idx];
			}
			op(child);
		}
	}
	template<class OP>
	void for_each_dest_rev(size_t parent, OP op) const {
		assert(parent < total_states());
		assert(!m_is_cross_edge[parent]);
		size_t bitpos = m_louds.select0(parent);
		assert(m_louds.rank0(bitpos) == parent);
		assert(m_louds.is0(bitpos));
		bitpos++;
		size_t child0 = bitpos - parent;
		size_t lcount = m_louds.one_seq_len(bitpos);
		assert(lcount <= sigma);
		assert(child0 + lcount <= total_states());
		for(size_t i = lcount; i > 0;) {
			size_t child = child0 + --i;
			if (m_is_cross_edge[child]) {
				size_t cross_idx = m_is_cross_edge.rank1(child);
				child = m_cross_edge_dst[cross_idx];
			}
			op(child);
		}
	}

	template<class DataIO>
	void dio_load(DataIO& dio) {
		THROW_STD(logic_error, "Not implemented");
	}
	template<class DataIO>
	void dio_save(DataIO& dio) const {
		THROW_STD(logic_error, "Not implemented");
	}
	template<class DataIO>
	friend void
	DataIO_loadObject(DataIO& dio, GenLoudsDFA& dfa) { dfa.dio_load(dio); }
	template<class DataIO>
	friend void
	DataIO_saveObject(DataIO& dio, const GenLoudsDFA& dfa) { dfa.dio_save(dio); }

	void finish_load_mmap(const DFA_MmapHeader*) override;
	long prepare_save_mmap(DFA_MmapHeader*, const void**) const override;

	typedef GenLoudsDFA MyType;
#include "ppi/for_each_suffix.hpp"
#include "ppi/match_key.hpp"
#include "ppi/match_path.hpp"
#include "ppi/match_prefix.hpp"
#include "ppi/accept.hpp"
#include "ppi/dfa_const_virtuals.hpp"
#include "ppi/adfa_iter.hpp"
#include "ppi/for_each_word.hpp"
#include "ppi/match_pinyin.hpp"
};

template<class RankSelect>
template<class DFA>
size_t GenLoudsDFA<RankSelect>::calc_cross_edge_num(const DFA& au) {
	size_t the_leaf_final_state = au.find_first_leaf();
	if (size_t(-1) != the_leaf_final_state) {
		if (au.is_pzip(the_leaf_final_state))
			the_leaf_final_state = size_t(-1);
	}
	using terark::AutoFree;
	febitvec         color(au.total_states(), 0);
	valvec  <size_t> stack(256, valvec_reserve());
	AutoFree<size_t> children(au.get_sigma());
	color.set1(initial_state);
	stack.push_back(initial_state);
	size_t cross_edge_num = 0;
	while (!stack.empty()) {
		size_t parent = stack.back(); stack.pop_back();
		size_t cnt = au.get_all_dest(parent, children);
		for(size_t i = 0; i < cnt; ++i) {
			size_t child = children[i];
			if (color.is0(child)) {
				color.set1(child);
				stack.push_back(child);
			}
			else if (child != the_leaf_final_state) {
				cross_edge_num++;
			}
		}
	}
	assert(cross_edge_num <= au.total_transitions() - au.total_states() + 1);
	return cross_edge_num;
}

template<class RankSelect>
template<class Au>
void GenLoudsDFA<RankSelect>::build_from(const Au& au, const NestLoudsTrieConfig& conf) {
	if (!au.is_dag()) {
		THROW_STD(invalid_argument, "au must be DAG");
	}
	if (au.get_sigma() > 256) {
		THROW_STD(invalid_argument, "Sigma for GenLoudsDFA must <= 256");
	}
//	if (au.num_free_states() > 0) {
//		THROW_STD(invalid_argument, "param au must not has free states");
//	}
	typedef typename Au::state_id_t au_state_id_t;
	// label_data[0] is unused
	size_t label_size = 1 + au.total_transitions() + mm_cmpestri_extra;
	// cross_edge_upp is max possible cross edge num
	size_t cross_edge_upp = au.total_transitions() - au.num_used_states() + 1;
	size_t final_leaf_num = 0;
	double cache_ratio = 0.0;
	if (const char* env = getenv("LOUDS_DFA_CACHE_RATIO")) {
		cache_ratio = atof(env);
		cache_ratio = std::min(1.0, cache_ratio);
		cache_ratio = std::max(0.0, cache_ratio);
	}
	valvec<uint32_t>  cache(au.total_transitions() * cache_ratio);
	valvec<uint32_t>  cross_edge_dst(cross_edge_upp, valvec_reserve());
	valvec<uint32_t>  cross_edge_map(au.total_states(), uint32_t(-1));
	RankSelect  is_cross_edge (au.total_transitions()+1);
	RankSelect  louds         (au.total_transitions()*2+2, valvec_reserve());
	RankSelect  l_is_pzip     (au.num_zpath_states() ? au.total_transitions() + 1 : 0);
	febitvec    l_is_term     (au.total_transitions()+1);
	AutoFree<byte_t> label_data(label_size +
		(conf.nestLevel ? 0 : au.total_zpath_len() + 1));
	AutoFree<CharTarget<size_t> > children(au.get_sigma());
	byte_t* zpath_data = label_data + label_size;
	size_t trans_idx = 0;
	size_t zpath_pos = 1; // zpath_data[0] is unused
	size_t znode_idx = 0;
	size_t the_leaf_final_state = au.find_first_leaf();
	if (size_t(-1) != the_leaf_final_state) {
		if (au.is_pzip(the_leaf_final_state))
			the_leaf_final_state = size_t(-1);
	}
	if (conf.nestLevel > 0) {
		m_zpath_trie = new NestLoudsTrieTpl<RankSelect>();
	} else {
		m_zpath_index.resize(au.total_zpath_len() - au.num_zpath_states() + 1);
	}
	SortableStrVec zpathStrVec;
	valvec<au_state_id_t> q1, q2;
	febitvec color(au.total_states(), 0);
	q1.push_back(initial_state);
	color.set1(initial_state);
	cross_edge_map.set(0, 0);
	size_t bfs_serial_no = 0; // is the louds node id
	louds.push_back(0);
	label_data[0] = 0; // unused
	while (!q1.empty()) {
		for(size_t i = 0; i < q1.size(); ++i) {
			size_t parent = q1[i];
			if (bfs_serial_no < cache.size()) {
				cache[bfs_serial_no] = louds.size();
			}
			if (au_state_id_t(-1) != parent) {
				if (au.is_pzip(parent)) {
					fstring zs = au.get_zpath_data(parent, NULL);
					assert(zs.size() >= 2);
					if (m_zpath_trie) {
						zpathStrVec.push_back(zs);
					}
					else {
						memcpy(zpath_data + zpath_pos, zs.data(), zs.size());
						m_zpath_index.set1(zpath_pos - znode_idx, zs.size() - 2);
						m_zpath_index.set0(zpath_pos - znode_idx + zs.size() - 2);
					}
					assert(l_is_pzip.size() == au.total_transitions() + 1);
					l_is_pzip.set1(bfs_serial_no);
					zpath_pos += zs.size();
					znode_idx++;
				}
				size_t n_children = au.get_all_move(parent, children);
				byte_t* mylabels = label_data + trans_idx + 1;
				memset(mylabels, 0, n_children);
				for(size_t j = 0; j < n_children; ++j) {
					size_t ch    = children[j].ch;
					size_t child = children[j].target;
					if (n_children < MIN_FAST_SEARCH_BYTES)
						label_data[trans_idx + 1] = ch;
					else {
						uint32_t mask = uint32_t(1) << ch%32;
						unaligned_save<uint32_t>(mylabels+POPCNT_BYTES, ch/32,
							unaligned_load<uint32_t>(mylabels+POPCNT_BYTES, ch/32) | mask);
					}
					if (color.is0(child)) {
						color.set1(child);
						q2.push_back(child);
						cross_edge_map.set(child, trans_idx+1);
					} else {
						if (child != the_leaf_final_state) {
							is_cross_edge.set1(trans_idx+1);
							cross_edge_dst.push_back(cross_edge_map[child]);
						} else {
							l_is_term.set1(trans_idx+1);
							final_leaf_num++;
						}
						q2.push_back(au_state_id_t(-1));
					}
					louds.push_back(1);
					trans_idx++;
				};
				if (n_children >= MIN_FAST_SEARCH_BYTES) {
					size_t popcnt_sum = 0;
					for (size_t i = 0; i < POPCNT_BYTES; ++i) {
						mylabels[i] = (byte_t)popcnt_sum;
						popcnt_sum += fast_popcount32(
							unaligned_load<uint32_t>(mylabels + POPCNT_BYTES, i));
					}
				}
				l_is_term.set(bfs_serial_no, au.is_term(parent));
			}
			louds.push_back(0);
			bfs_serial_no++;
		}
		q1.swap(q2);
		q2.erase_all();
	}
#if defined(NDEBUG)
	cross_edge_map.clear();
#endif
	m_cross_edge_dst.build_from(cross_edge_dst);
	cross_edge_dst.clear();
	memset(label_data + trans_idx + 1, 0, mm_cmpestri_extra);
	m_cache.build_from(cache);

	if (m_zpath_trie) {
		valvec<size_t> zpathLinkVec(zpathStrVec.size(), SIZE_MAX);
		m_zpath_trie->build_strpool(zpathStrVec, zpathLinkVec, conf);
		m_zpath_id.build_from(zpathLinkVec);
		m_zpNestLevel = m_zpath_trie->nest_level();
	}
	else {
		zpath_data[0] = 0; // unused byte, set it
		m_zpath_index.build_cache(true, false);
		m_zpath_data = zpath_data;
		m_zpNestLevel = 0;
	}

	if (const char* env = getenv("LOUDS_DFA_PRINT_STAT")) {
	  int ival = atoi(env);
	  if (ival) {
		fprintf(stderr, "au.total_states()=%zd\n", au.total_states());
		fprintf(stderr, "au.num_free_states()=%zd\n", au.num_free_states());
		fprintf(stderr, "au.num_used_states()=%zd\n", au.num_used_states());
		fprintf(stderr, "au.total_transitions()=%zd\n", au.total_transitions());
		fprintf(stderr, "au.num_zpath_states()=%zd\n", au.num_zpath_states());
		fprintf(stderr, "au.total_zpath_len()=%lld\n", (long long)au.total_zpath_len());
		fprintf(stderr, "real_zpath_len=%zd\n", zpath_pos);
		fprintf(stderr, "cross_edge_num=%zd\n", m_cross_edge_dst.size());
		fprintf(stderr, "cross_edge_upp=%zd\n", cross_edge_upp);
		fprintf(stderr, "cross_edge_mem=%zd\n", m_cross_edge_dst.mem_size());
		fprintf(stderr, "cross_edge_bit=%zd\n", m_cross_edge_dst.uintbits());
		fprintf(stderr, "final_leaf_num=%zd\n", final_leaf_num);
		fprintf(stderr, "bfs_serial_no=%zd\n", bfs_serial_no);
		fprintf(stderr, "trans_idx=%zd\n", trans_idx);
		fprintf(stderr, "znode_idx=%zd\n", znode_idx);
		fprintf(stderr, "cache_bit=%zd\n", m_cache.uintbits());
		fprintf(stderr, "cache_num=%zd\n", m_cache.size());
		fprintf(stderr, "cache_mem=%zd\n", m_cache.mem_size());
		fprintf(stderr, "zpath_trie.memsize=%zd\n", m_zpath_trie ? m_zpath_trie->mem_size() : 0);
		fprintf(stderr, "zpath_link.memsize=%zd\n", m_zpath_id.mem_size());
	  }
	}
	assert(au.total_transitions() == trans_idx);
	assert(m_cross_edge_dst.size() + final_leaf_num == cross_edge_upp);
	assert(cross_edge_upp + au.num_used_states()-1 == au.total_transitions());
	assert(au.total_zpath_len()+1 == zpath_pos);
	assert(au.total_transitions() == bfs_serial_no-1);
	assert(au.num_zpath_states () == znode_idx);
	assert(2 * bfs_serial_no == louds.size());

	is_cross_edge.build_cache(false, false);
	l_is_pzip    .build_cache(false, false);
	louds        .build_cache(true , false);

	m_zpath_states = au.num_zpath_states();
	m_label_data = label_data.release();
	m_louds.swap(louds);
	m_is_pzip.swap(l_is_pzip);
	m_is_term.swap(l_is_term);
	m_is_cross_edge.swap(is_cross_edge);
	m_total_zpath_len = au.total_zpath_len();
}

typedef GenLoudsDFA<rank_select_se> LoudsDFA_SE;
typedef GenLoudsDFA<rank_select_il> LoudsDFA_IL;
typedef GenLoudsDFA<rank_select_se_512> LoudsDFA_SE_512;

} // namespace terark
