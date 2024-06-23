#pragma once

#include <terark/fsa/automata_basic.hpp>
#include <terark/util/sortable_strvec.hpp>

namespace terark {
struct SuffixTrieCacheState : public State32 {
	static const size_t MaxDepth = MAX_ZPATH_LEN;
	size_t getZstrLen() const { return this->padding_bits; }
	void setZstrLen(size_t pos) {
		assert(pos < MaxDepth);
		this->padding_bits = pos;
	}
	uint32_t suffixLow = -1;
	uint32_t suffixHig = -1;
};
class TERARK_DLL_EXPORT SuffixTrieCacheDFA : public AutomataAsBaseDFA<SuffixTrieCacheState>  {
public:
	valvec<int>     m_sa;
	const SortableStrVec* m_strVec = nullptr;
	void build_sa(SortableStrVec& strVec);
	void bfs_build_cache(size_t minTokenLen, size_t minFreq, size_t maxDepth);
	void pfs_build_cache(size_t minTokenLen, size_t minFreq);

	struct DfsBuildLoopParam;
	void dfs_build_cache(size_t minTokenLen, size_t minFreq, size_t maxDepth);
	void dfs_build_cache_loop(size_t lo, size_t hi, size_t depth, size_t state, DfsBuildLoopParam&);
	void dot_write_one_state(FILE* fp, size_t ls, const char* ext_attr) const override;
	struct BfsQueueElem;
	struct MatchStatus {
		size_t lo;
		size_t hi;
		size_t depth;
		size_t state;
		size_t freq() const { return hi - lo; }
	};
	MatchStatus sa_root() const {
		assert(m_sa.size() == m_strVec->str_size());
		MatchStatus st;
		st.lo = 0;
		st.hi = m_strVec->str_size();
		st.depth = 0;
		st.state = 0;
		return st;
	}
	bool sa_state_move(const MatchStatus& curr, byte_t ch, MatchStatus* next) const;
	void sa_print_match(const MatchStatus&);
	void sa_print_stat() const;

	MatchStatus sa_match_max_cover(fstring, size_t minLen, size_t minFreq) const;
	MatchStatus sa_match_max_score(fstring, size_t minLen, size_t minFreq) const;
	MatchStatus sa_match_max_length(fstring, size_t minFreq) const;
	MatchStatus sa_match_max_length_slow(fstring, size_t minFreq) const;

	size_t make_lz_trie(fstring fpath, size_t minLen) const;

	size_t sa_lower_bound(size_t lo, size_t hi, size_t depth, byte_t ch) const;
	size_t sa_upper_bound(size_t lo, size_t hi, size_t depth, byte_t ch) const;
	std::pair<size_t, size_t>
		   sa_equal_range(size_t lo, size_t hi, size_t depth, byte_t ch) const;

	fstring get_zstr(size_t curpos, const SuffixTrieCacheState& s) const {
		const size_t  zpos = m_sa[s.suffixLow] + curpos;
		const byte_t* text = m_strVec->m_strpool.data();
		return fstring(text + zpos, s.getZstrLen());
	}
	fstring get_zstr(size_t curpos, size_t s) const {
		assert(s < states.size());
		return get_zstr(curpos, this->states[s]);
	}
};

} // namespace terark
