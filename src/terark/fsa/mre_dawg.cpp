#include "mre_match.hpp"
#include "fsa.hpp"
#include "nest_trie_dawg.hpp"
#include <setjmp.h>
#include <terark/util/throw.hpp>
#include <terark/util/unicode_iterator.hpp>

namespace terark {

namespace detail {
    struct DawgOnMatchShortest {
        size_t len;
        size_t nth;
        jmp_buf jmp;
        void operator()(size_t len, size_t nth) {
            this->len = len;
            this->nth = nth;
            longjmp(jmp, 1);
        }
    };
}
using namespace detail;

template<class Dawg>
class MultiRegexFullMatchDawg : public MultiRegexFullMatch {
public:
    explicit MultiRegexFullMatchDawg(const BaseDFA* d) {
        m_dfa = d;
    }
    virtual	MultiRegexFullMatch* clone() const override {
        return new MultiRegexFullMatchDawg(m_dfa);
    }

    ///@{ longest match from start, return value is matched len
    size_t match(fstring text) override {
        m_regex_idvec.erase_all();
        const Dawg* d = static_cast<const Dawg*>(m_dfa);
        size_t len = 0;
        size_t id = size_t(-1);
        if (d->match_dawg_l(text, &len, &id)) {
            m_regex_idvec.push_back(int(id));
            return len;
        }
        return 0;
    }
    size_t match(fstring text, const ByteTR& tr) override {
        m_regex_idvec.erase_all();
        const Dawg* d = static_cast<const Dawg*>(m_dfa);
        size_t len = 0;
        size_t id = size_t(-1);
        if (d->match_dawg_l(text, &len, &id, tr)) {
            m_regex_idvec.push_back(int(id));
            return len;
        }
        return 0;
    }
    size_t match(fstring text, const byte_t* tr) override {
        m_regex_idvec.erase_all();
        const Dawg* d = static_cast<const Dawg*>(m_dfa);
        size_t len = 0;
        size_t id = size_t(-1);
        if (d->match_dawg_l(text, &len, &id, tr)) {
            m_regex_idvec.push_back(int(id));
            return len;
        }
        return 0;
    }
    ///@}

    ///@{ shortest match from start, return value is matched len
    template<class TR>
    size_t tpl_shortest_match(fstring text, TR tr) {
        m_regex_idvec.erase_all();
        const Dawg* d = static_cast<const Dawg*>(m_dfa);
        MatchContext ctx;
        DawgOnMatchShortest func;
        if (setjmp(func.jmp) == 0) {
            d->template tpl_match_dawg<DawgOnMatchShortest&, TR>(ctx, 0, text, func, tr);
            return 0;
        }
        else {
            m_regex_idvec.push_back(int(func.nth));
            return func.len;
        }
    }
    size_t shortest_match(fstring text) override {
        return tpl_shortest_match(text, IdentityTR());
    }
    size_t shortest_match(fstring text, const ByteTR& tr) override {
        return tpl_shortest_match<const ByteTR&>(text, tr);
    }
    size_t shortest_match(fstring text, const byte_t* tr) override {
        return tpl_shortest_match(text, TableTranslator(tr));
    }
    ///@}

    ///@{ match from start, get all id of all matched regex-es which
    ///   may be ended at any position
    ///@returns number of matched regex, same as this->size()
    /// also fill m_regex_idvec, this is stronger than that for general regex
    template<class TR>
    size_t tpl_match_all(fstring text, TR tr) {
        m_cur_match.erase_all();
        m_regex_idvec.erase_all();
        const Dawg* d = static_cast<const Dawg*>(m_dfa);
        MatchContext ctx;
        auto func = [this](size_t len, size_t nth) {
            this->m_cur_match.emplace_back(int(len), int(nth));
        };
        d->tpl_match_dawg(ctx, 0, text, func, tr);
        size_t n = m_cur_match.size();
        m_regex_idvec.resize_no_init(n);
        for (size_t i = 0; i < n; ++i)
            m_regex_idvec[i] = m_cur_match[i].regex_id;
        return n;
    }
    size_t match_all(fstring text) override {
        return tpl_match_all(text, IdentityTR());
    }
    size_t match_all(fstring text, const ByteTR& tr) override {
        return tpl_match_all<const ByteTR&>(text, tr);
    }
    size_t match_all(fstring text, const byte_t* tr) override {
        return tpl_match_all(text, TableTranslator(tr));
    }
    size_t match_all_len(fstring text) override {
        return match_all(text);
    }
    size_t match_all_len(fstring text, const ByteTR& tr) override {
        return match_all(text, tr);
    }
    size_t match_all_len(fstring text, const byte_t* tr) override {
        return match_all(text, tr);
    }
    ///@}

#define IMPLEMENT_find_all_len_dawg(StepBytes, MatchCall) \
	m_all_match.erase_all(); \
    m_regex_idvec.erase_all(); \
    const Dawg* d = static_cast<const Dawg*>(m_dfa); \
	const char* pos = text.begin(); \
	const char* end = text.end(); \
	while (pos < end) { \
        MatchContext ctx; \
		fstring suffix(pos, end); \
        int ipos = int(pos - text.begin()); \
        auto func = [this,ipos](size_t len, size_t nth) { \
            this->m_all_match.emplace_back(ipos, int(len), int(nth)); \
        }; \
        MatchCall; \
		pos += StepBytes; \
	} \
    return m_all_match.size()

    size_t utf8_find_all_len(fstring text) override {
        IMPLEMENT_find_all_len_dawg(utf8_byte_count(*pos),
            d->tpl_match_dawg(ctx, 0, suffix, func, IdentityTR()));
    }
    size_t utf8_find_all_len(fstring text, const ByteTR& tr) override {
        IMPLEMENT_find_all_len_dawg(utf8_byte_count(*pos),
            d->tpl_match_dawg(ctx, 0, suffix, func, tr));
    }
    size_t utf8_find_all_len(fstring text, const byte_t* tr) override {
        IMPLEMENT_find_all_len_dawg(utf8_byte_count(*pos),
            d->tpl_match_dawg(ctx, 0, suffix, func, TableTranslator(tr)));
    }
    size_t byte_find_all_len(fstring text) override {
        IMPLEMENT_find_all_len_dawg(1,
            d->tpl_match_dawg(ctx, 0, suffix, func, IdentityTR()));
    }
    size_t byte_find_all_len(fstring text, const ByteTR& tr) override {
        IMPLEMENT_find_all_len_dawg(1,
            d->tpl_match_dawg(ctx, 0, suffix, func, tr));
    }
    size_t byte_find_all_len(fstring text, const byte_t* tr) override {
        IMPLEMENT_find_all_len_dawg(1,
            d->tpl_match_dawg(ctx, 0, suffix, func, TableTranslator(tr)));
    }
};

MultiRegexFullMatch* MultiRegexFullMatch_dawg(const BaseDFA* dfa) {

#define TryDAWG(Dawg) \
	if (const Dawg* d = dynamic_cast<const Dawg*>(dfa)) \
		return new MultiRegexFullMatchDawg<Dawg>(d)

    TryDAWG(NestLoudsTrieDAWG_SE_256);
    TryDAWG(NestLoudsTrieDAWG_SE_512);
    TryDAWG(NestLoudsTrieDAWG_IL_256);

    TryDAWG(NestLoudsTrieDAWG_SE_512_64);

    TryDAWG(NestLoudsTrieDAWG_Mixed_SE_512);
    TryDAWG(NestLoudsTrieDAWG_Mixed_IL_256);
    TryDAWG(NestLoudsTrieDAWG_Mixed_XL_256);

    TryDAWG(NestLoudsTrieDAWG_SE_256_32_FL);
    TryDAWG(NestLoudsTrieDAWG_SE_512_32_FL);
    TryDAWG(NestLoudsTrieDAWG_IL_256_32_FL);
    TryDAWG(NestLoudsTrieDAWG_SE_512_64_FL);

    TryDAWG(NestLoudsTrieDAWG_Mixed_SE_512_32_FL);
    TryDAWG(NestLoudsTrieDAWG_Mixed_IL_256_32_FL);
    TryDAWG(NestLoudsTrieDAWG_Mixed_XL_256_32_FL);

    return NULL;
}

} // namespace terark

