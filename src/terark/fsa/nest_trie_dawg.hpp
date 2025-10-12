#pragma once

#include "nest_louds_trie.hpp"
#include <terark/util/autofree.hpp>
#if defined(TerarkFSA_HighPrivate)
#include "dfa_algo.hpp"
#include "graph_walker.hpp"
#endif // TerarkFSA_HighPrivate

namespace terark {

template<class NestTrie, bool IsRankSelect2>
class TERARK_DLL_EXPORT NestTrieDAWG_IsTerm {
protected:
	typedef typename NestTrie::rank_select_t rank_select_t;
	NestTrie* m_trie;
	rank_select_t  m_is_term;
	rank_select_t& getIsTerm() { return m_is_term; }
	const rank_select_t& getIsTerm() const { return m_is_term; }
public:
    bool is_term(size_t s) const noexcept {
        assert(s < m_is_term.size());
        return m_is_term[s];
    }
    inline bool is_term2(const NestTrie* trie, size_t s) const noexcept {
        assert(trie == m_trie);
        assert(s < m_is_term.size());
        return m_is_term[s];
    }
    inline void prefetch_term_bit(const NestTrie* trie, size_t s) const noexcept {
        assert(trie == m_trie);
        assert(s < m_is_term.size());
        m_is_term.prefetch_bit(s);
    }
    inline size_t term_rank1(const NestTrie* trie, size_t s) const noexcept {
        assert(trie == m_trie);
        assert(s < m_is_term.size());
        return m_is_term.rank1(s);
    }
    void swap(NestTrieDAWG_IsTerm& y) noexcept {
		std::swap(m_trie, y.m_trie);
		m_is_term.swap(y.m_is_term);
	}
	void risk_release_ownership() noexcept {
		m_trie->risk_release_ownership();
		m_is_term.risk_release_ownership();
	}
	size_t mem_size() const noexcept {
		return m_trie->mem_size() + m_is_term.mem_size();
	}
};
template<class NestTrie>
class TERARK_DLL_EXPORT NestTrieDAWG_IsTerm<NestTrie, true> {
protected:
	NestTrie* m_trie;
	typedef typename NestTrie::rank_select2_t rank_select2_t;
	typename rank_select2_t::rank_select_view_1&
	getIsTerm() noexcept {
		return m_trie->m_is_link.template get<1>();
	}
	const typename rank_select2_t::rank_select_view_1&
	getIsTerm() const noexcept {
		return m_trie->m_is_link.template get<1>();
	}
public:
    bool is_term(size_t s) const noexcept {
        assert(s < m_trie->m_is_link.template get<1>().size());
        return m_trie->m_is_link.template get<1>().is1(s);
    }
    inline bool is_term2(const NestTrie* trie, size_t s) const noexcept {
        assert(trie == m_trie);
        assert(s < trie->m_is_link.template get<1>().size());
        return trie->m_is_link.template get<1>().is1(s);
    }
    inline void prefetch_term_bit(const NestTrie* trie, size_t s) const noexcept {
        assert(trie == m_trie);
        assert(s < trie->m_is_link.template get<1>().size());
        trie->m_is_link.template get<1>().prefetch_bit(s);
    }
    inline size_t term_rank1(const NestTrie* trie, size_t s) const noexcept {
        assert(trie == m_trie);
        assert(s < trie->m_is_link.template get<1>().size());
        return trie->m_is_link.template get<1>().rank1(s);
    }
    void swap(NestTrieDAWG_IsTerm<NestTrie, true>& y) noexcept {
		std::swap(m_trie, y.m_trie);
	}
	void risk_release_ownership() noexcept {
		m_trie->risk_release_ownership();
	}
	size_t mem_size() const { return m_trie->mem_size(); }
};

template<class NestTrie, class DawgType>
class TERARK_DLL_EXPORT NestTrieDAWG : public MatchingDFA, public DawgType
	, public FSA_Cache
	, public NestTrieDAWG_IsTerm<NestTrie, NestTrie::is_link_rs_mixed::value>
{
protected:
	typedef NestTrieDAWG_IsTerm<NestTrie, NestTrie::is_link_rs_mixed::value> IsTermRep;
    typedef typename NestTrie::index_t index_t;
	size_t m_zpNestLevel;
	NTD_CacheTrie* m_cache;
	using DawgType::n_words;
	using IsTermRep::m_trie;
    using IsTermRep::getIsTerm;

    template<class StrVecType>
    void build_from_tpl(StrVecType&, const NestLoudsTrieConfig&);

    void build_term_bits(const valvec<size_t>& linkVec);

public:
    using IsTermRep::is_term;
	using DawgType::null_word;
	typedef NestTrie trie_type;
	typedef typename NestTrie::is_link_rs_mixed  is_link_rs_mixed;
	typedef size_t state_id_t;
	typedef size_t state_t;
	typedef size_t State;
	typedef size_t transition_t;
	typedef typename NestTrie::StateMoveContext  StateMoveContext;
	typedef typename NestTrie::template Iterator<NestTrieDAWG>  Iterator;
//  friend typename               Iterator; // g++ fail
    friend typename NestTrieDAWG::Iterator; // g++ ok

	static const size_t nil_state = size_t(-1);
	static const size_t max_state = size_t(-2);
	enum { sigma = 256 };

	~NestTrieDAWG();
	NestTrieDAWG();
	NestTrieDAWG(const NestTrieDAWG&);
	NestTrieDAWG& operator=(const NestTrieDAWG&);

	size_t iter_mem_size() const;
	void cons_iter(void* mem) const;

	void swap(NestTrieDAWG& y) noexcept;

    size_t max_strlen() const noexcept final { return m_trie->m_max_strlen; }
	size_t total_states() const { return getIsTerm().size(); }
	bool has_freelist() const override; // return false

	size_t state_move(size_t, auchar_t ch) const noexcept;
	size_t state_move_slow(size_t, auchar_t ch, StateMoveContext& ctx) const noexcept;
	size_t state_move_fast(size_t, auchar_t ch, size_t n_children, size_t child0) const noexcept;
	size_t state_move_fast(size_t parent, auchar_t ch, const StateMoveContext& ctx) const noexcept {
		return state_move_fast(parent, ch, ctx.n_children, ctx.child0);
	}

	bool   is_free(size_t s) const noexcept { return false; }
	bool   is_pzip(size_t s) const noexcept { return m_trie->is_pzip(s); }
	bool   v_has_children(size_t) const override final;
	size_t v_num_children(size_t s) const override final;
	size_t v_gnode_states() const override final;
	size_t zp_nest_level() const override final;

	fstring get_zpath_data(size_t, MatchContext*) const noexcept;
	size_t mem_size() const override final;

	void build_from(SortableStrVec&, const NestLoudsTrieConfig&);
	void build_from(SortThinStrVec&, const NestLoudsTrieConfig&);
	void build_from(FixedLenStrVec&, const NestLoudsTrieConfig&);
	void build_from(VoSortedStrVec&, const NestLoudsTrieConfig&);
	void build_from(ZoSortedStrVec&, const NestLoudsTrieConfig&);
	void build_from(DoSortedStrVec&, const NestLoudsTrieConfig&);
	void build_from(QoSortedStrVec&, const NestLoudsTrieConfig&);

	void build_with_id(SortableStrVec&, valvec<size_t>& idvec, const NestLoudsTrieConfig&);

	template<class OP>
	void for_each_move(size_t parent, OP op) const {
		assert(parent < getIsTerm().size());
		m_trie->template for_each_move<OP>(parent, op);
	}

	template<class OP>
	void for_each_dest(size_t parent, OP op) const {
		assert(parent < getIsTerm().size());
		m_trie->template for_each_dest<OP>(parent, op);
	}

	template<class OP>
	void for_each_dest_rev(size_t parent, OP op) const {
		assert(parent < getIsTerm().size());
		m_trie->template for_each_dest_rev<OP>(parent, op);
	}

	size_t state_to_word_id(size_t state) const noexcept {
		assert(state < m_trie->total_states());
		return getIsTerm().rank1(state);
	}
	size_t v_state_to_word_id(size_t state) const override;

    size_t state_to_dict_rank(size_t state) const override;
    size_t dict_rank_to_state(size_t rank) const noexcept;

// DAWG functions:
	enum { is_compiled = 1 }; // for dawg
	typedef BaseDAWG::OnMatchDAWG OnMatchDAWG;
	using DawgType::index;
    using BaseDAWG::lower_bound;
	using DawgType::nth_word;
	using BaseDAWG::match_dawg;
	using BaseDAWG::match_dawg_l;
	size_t index(MatchContext&, fstring) const noexcept override final;
	size_t index(fstring) const noexcept override final;
	bool has_da_cache() const noexcept { return m_cache != nullptr; }
	template<bool HasLink, bool TryDACache = true>
	size_t index_impl(fstring) const noexcept terark_pure_func;
	size_t index_impl_00(fstring) const noexcept terark_pure_func;
	size_t index_impl_01(fstring) const noexcept terark_pure_func;
	size_t index_impl_10(fstring) const noexcept terark_pure_func;
	size_t index_impl_11(fstring) const noexcept terark_pure_func;
	template<bool HasLink>
	size_t index_impl_ctx(MatchContext&, fstring) const noexcept;

    void lower_bound(MatchContext&, fstring, size_t* index, size_t* dict_rank) const noexcept override final;
    size_t index_begin() const noexcept;
    size_t index_end() const noexcept;
    size_t index_next(size_t nth) const noexcept;
    size_t index_prev(size_t nth) const noexcept;

	void nth_word(MatchContext&, size_t, std::string*) const noexcept override final;
	void nth_word(size_t, std::string*) const noexcept override final;
	void nth_word(MatchContext&, size_t, valvec<byte_t>*) const noexcept;
	void nth_word(size_t, valvec<byte_t>*) const noexcept;

	void get_random_keys_append(SortableStrVec* keys, size_t max_keys) const override;

	DawgIndexIter dawg_lower_bound(MatchContext&, fstring) const noexcept override;

	template<class OnMatch, class TR>
	size_t tpl_match_dawg
(MatchContext& ctx, size_t base_nth, fstring str, OnMatch on_match, TR tr)
	const noexcept {
        assert(m_trie->m_is_link.max_rank1() == this->m_zpath_states);
        if (m_trie->m_is_link.max_rank1() > 0)
            return tpl_match_dawg_impl<OnMatch, TR, true>(ctx, base_nth, str, on_match, tr);
        else
            return tpl_match_dawg_impl<OnMatch, TR, false>(ctx, base_nth, str, on_match, tr);
    }
	template<class OnMatch, class TR, bool HasLink>
	size_t tpl_match_dawg_impl
(MatchContext& ctx, size_t base_nth, fstring str, OnMatch on_match, TR tr)
	const noexcept {
		if (0 == ctx.root) {
			assert(0 == ctx.pos);
			assert(0 == ctx.zidx);
			assert(0 == base_nth);
			const auto  trie = m_trie;
			size_t curr = 0;
			size_t i = 0;
			for (; nil_state != curr; ++i) {
                this->prefetch_term_bit(trie, curr);
				if (HasLink && trie->is_pzip(curr)) {
                    intptr_t matchLen = trie->matchZpath(curr,
                              (const byte_t*)str.p + i, str.n - i);
                    if (terark_likely(matchLen > 0)) {
                        i += matchLen;
                    } else {
                        i -= matchLen;
                        goto Done;
                    }
				}
				if (this->is_term2(trie, curr)) {
					size_t word_idx = this->term_rank1(trie, curr);
					on_match(i, word_idx);
				}
				if (str.size() == i)
					break;
				byte_t ch = (byte_t)tr((byte_t)str.p[i]);
				curr = trie->template state_move_smart<HasLink>(curr, ch);
			}
		Done:
			ctx.pos = i;
			ctx.root = curr;
			return i;
		}
		else {
			return m_trie->template tpl_match_dawg<OnMatch, TR>
				(ctx, base_nth, str, on_match, tr, getIsTerm());
		}
	}
// End DAWG functions

	void finish_load_mmap(const DFA_MmapHeader*) override;
	long prepare_save_mmap(DFA_MmapHeader*, const void**) const override;
	void str_stat(std::string* s) const override;

	// FSA_Cache overrides
	bool has_fsa_cache() const override;
	bool build_fsa_cache(double ratio, const char* WalkMethod) override;
	void print_fsa_cache_stat(FILE*) const override;

	// extended
	void debug_equal_check(const NestTrieDAWG& y) const { m_trie->debug_equal_check(*y.m_trie); }

	typedef NestTrieDAWG MyType;
#include "ppi/dfa_const_virtuals.hpp"
#include "ppi/dawg_const_virtuals.hpp"

#if 0
#include "ppi/adfa_iter.hpp"
#else
virtual ADFA_LexIterator* adfa_make_iter(size_t root = initial_state) const override;
virtual ADFA_LexIterator16* adfa_make_iter16(size_t root = initial_state) const override;
#endif

#if defined(TerarkFSA_HighPrivate)
#include "ppi/for_each_suffix.hpp"
#include "ppi/match_key.hpp"
#include "ppi/match_path.hpp"
#include "ppi/match_prefix.hpp"
#include "ppi/accept.hpp"
#include "ppi/for_each_word.hpp"
#include "ppi/match_pinyin.hpp"
#endif // TerarkFSA_HighPrivate
};

#if defined(TerarkFSA_HighPrivate)
template<class NestTrie>
class TERARK_DLL_EXPORT SuffixCoutableNestTrieDAWG
	: public NestTrieDAWG<NestTrie, SuffixCountableDAWG>
{
	typedef NestTrieDAWG<NestTrie, SuffixCountableDAWG> super;
	using super::m_trie;
	using super::m_is_term;
	using super::is_compiled;
	using super::nil_state;
	using super::max_state;
public:
	size_t inline_suffix_cnt(size_t root) const { return suffix_cnt(root); }
	size_t suffix_cnt(size_t) const override;
#include "ppi/virtual_suffix_cnt.hpp"
};
#endif // TerarkFSA_HighPrivate

TERARK_NAME_TYPE(NestLoudsTrieDAWG_SE_256, NestTrieDAWG<NestLoudsTrie_SE_256, BaseDAWG>);
TERARK_NAME_TYPE(NestLoudsTrieDAWG_SE_512, NestTrieDAWG<NestLoudsTrie_SE_512, BaseDAWG>);
TERARK_NAME_TYPE(NestLoudsTrieDAWG_IL_256, NestTrieDAWG<NestLoudsTrie_IL_256, BaseDAWG>);

TERARK_NAME_TYPE(NestLoudsTrieDAWG_SE_512_64, NestTrieDAWG<NestLoudsTrie_SE_512_64, BaseDAWG>);

TERARK_NAME_TYPE(NestLoudsTrieDAWG_Mixed_SE_512, NestTrieDAWG<NestLoudsTrie_Mixed_SE_512, BaseDAWG>);
TERARK_NAME_TYPE(NestLoudsTrieDAWG_Mixed_IL_256, NestTrieDAWG<NestLoudsTrie_Mixed_IL_256, BaseDAWG>);
TERARK_NAME_TYPE(NestLoudsTrieDAWG_Mixed_XL_256, NestTrieDAWG<NestLoudsTrie_Mixed_XL_256, BaseDAWG>);

// FastLabel = true
TERARK_NAME_TYPE(NestLoudsTrieDAWG_SE_256_32_FL, NestTrieDAWG<NestLoudsTrie_SE_256_32_FL, BaseDAWG>);
TERARK_NAME_TYPE(NestLoudsTrieDAWG_SE_512_32_FL, NestTrieDAWG<NestLoudsTrie_SE_512_32_FL, BaseDAWG>);
TERARK_NAME_TYPE(NestLoudsTrieDAWG_IL_256_32_FL, NestTrieDAWG<NestLoudsTrie_IL_256_32_FL, BaseDAWG>);
TERARK_NAME_TYPE(NestLoudsTrieDAWG_SE_512_64_FL, NestTrieDAWG<NestLoudsTrie_SE_512_64_FL, BaseDAWG>);

TERARK_NAME_TYPE(NestLoudsTrieDAWG_Mixed_SE_512_32_FL, NestTrieDAWG<NestLoudsTrie_Mixed_SE_512_32_FL, BaseDAWG>);
TERARK_NAME_TYPE(NestLoudsTrieDAWG_Mixed_IL_256_32_FL, NestTrieDAWG<NestLoudsTrie_Mixed_IL_256_32_FL, BaseDAWG>);
TERARK_NAME_TYPE(NestLoudsTrieDAWG_Mixed_XL_256_32_FL, NestTrieDAWG<NestLoudsTrie_Mixed_XL_256_32_FL, BaseDAWG>);

/*
TERARK_NAME_TYPE(NestLoudsTrieDAWG_Mixed_IL_256_32_41_FL, NestTrieDAWG<NestLoudsTrie_Mixed_IL_256_32_41_FL, BaseDAWG>);
TERARK_NAME_TYPE(NestLoudsTrieDAWG_Mixed_XL_256_32_41_FL, NestTrieDAWG<NestLoudsTrie_Mixed_XL_256_32_41_FL, BaseDAWG>);
*/

/*
TERARK_NAME_TYPE(DfudsPatriciaTrie_SE, SuffixCoutableNestTrieDAWG<NestDfudsTrie_SE>);
TERARK_NAME_TYPE(DfudsPatriciaTrie_IL, SuffixCoutableNestTrieDAWG<NestDfudsTrie_IL>);
*/

} // namespace terark
