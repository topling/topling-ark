#include "mre_match.hpp"
#include "fsa.hpp"
#include "aho_corasick.hpp"
#include <setjmp.h>
#include <terark/util/throw.hpp>
#include <terark/util/unicode_iterator.hpp>

namespace terark {

struct MRE_AC_OnHitAll {
    valvec<MultiRegexFullMatch::PosLenRegexID>* m_all_match;
    const BaseAC* m_ac;
    typedef BaseAC::word_id_t WordID;
    void operator()(size_t beg, const WordID* pWords, size_t nWords, size_t)
    const {
        auto p = m_all_match->grow_no_init(nWords);
        for(size_t i = 0; i < nWords; ++i) {
            auto   wid = pWords[i];
            size_t wlen = m_ac->wlen(wid);
            p[i].regex_id = int(wid);
            p[i].pos = int(beg);
            p[i].len = int(wlen);
        }
    }
};

template<class AC>
class MultiRegexFullMatchAC : public MultiRegexFullMatch {
public:
    typedef typename AC::word_id_t WordID;

    explicit MultiRegexFullMatchAC(const BaseDFA* d) {
        m_dfa = d;
    }
    virtual	MultiRegexFullMatch* clone() const override {
        return new MultiRegexFullMatchAC(m_dfa);
    }

    ///@{ longest match from start, return value is matched len
    template<class TR>
    size_t tpl_match(fstring text, TR tr) {
        const AC* ac = static_cast<const AC*>(m_dfa);
        m_all_match.resize(1, {-1, -1, -1});
        m_regex_idvec.erase_all();
        auto onHit = [ac,this]
        (size_t beg, const WordID* pWords, size_t, size_t) {
            auto wid = pWords[0]; // longest
            int  len = ac->wlen(wid);
            this->m_all_match[0] = PosLenRegexID(int(beg), len, wid);
        };
        ac->tpl_ac_scan_rev(text, onHit, tr);
        if (0 == m_all_match[0].pos) {
            m_regex_idvec.push_back(m_all_match[0].regex_id);
            return m_all_match[0].len;
        }
        m_all_match.erase_all();
        return 0;
    }
    size_t match(fstring text) override {
        return tpl_match(text, IdentityTR());
    }
    size_t match(fstring text, const ByteTR& tr) override {
        return tpl_match(text, tr);
    }
    size_t match(fstring text, const byte_t* tr) override {
        return tpl_match(text, TableTranslator(tr));
    }
    ///@}

    ///@{ shortest match from start, return value is matched len
    template<class TR>
    size_t tpl_shortest_match(fstring text, TR tr) {
        const AC* ac = static_cast<const AC*>(m_dfa);
        m_all_match.resize(1, {-1, -1, -1});
        m_regex_idvec.erase_all();
        auto onHit = [ac,this]
        (size_t  beg, const WordID* pWords, size_t nWords, size_t) {
            auto wid = pWords[nWords-1]; // shortest
            int  len = ac->wlen(wid);
            this->m_all_match[0] = {PosLenRegexID(int(beg), len, wid)};
        };
        ac->tpl_ac_scan_rev(text, onHit, tr);
        if (0 == m_all_match[0].pos) {
            m_regex_idvec.push_back(m_all_match[0].regex_id);
            return m_all_match[0].len;
        }
        m_all_match.erase_all();
        return 0;
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
    /// implementation: find last by tpl_ac_scan_rev
    template<class TR>
    size_t tpl_match_all(fstring text, TR tr) {
        const AC* ac = static_cast<const AC*>(m_dfa);
        m_all_match.erase_all();
        m_regex_idvec.erase_all();
        auto onHit = [ac,this]
        (size_t beg, const WordID* pWords, size_t nWords, size_t) {
            this->m_all_match.resize_no_init(nWords);
            for(size_t i = 0; i < nWords; ++i) {
                auto wid = pWords[i];
                int  len = ac->wlen(wid);
                this->m_all_match[i] = PosLenRegexID(int(beg), len, wid);
            }
        };
        ac->tpl_ac_scan_rev(text, onHit, tr);
        size_t n = m_all_match.size();
        if (n && 0 == m_all_match[0].pos) {
            m_regex_idvec.resize_no_init(n);
            for (size_t i = 0; i < n; ++i)
                m_regex_idvec[i] = m_all_match[i].regex_id;
        }
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
    ///@}

    template<class TR>
    PosLen tpl_byte_find_first(fstring text, TR tr) {
        const AC* ac = static_cast<const AC*>(m_dfa);
        m_all_match.erase_all();
        m_regex_idvec.erase_all();
        PosLenRegexID first(-1, -1, -1);
        auto onHit = [&first,ac]
        (size_t beg, const WordID* pWords, size_t nWords, size_t) {
            auto  wid = pWords[0]; // longest
            first.len = int(ac->wlen(wid));
            first.pos = int(beg);
            first.regex_id = int(wid);
        };
        ac->tpl_ac_scan_rev(text, onHit, tr);
        if (-1 == first.pos) {
            return PosLen(-1, -1);
        }
        else {
            m_regex_idvec.push_back(first.regex_id);
            return PosLen(first.pos, first.len);
        }
    }
    PosLen utf8_find_first(fstring text) override {
        return byte_find_first(text);
    }
    PosLen utf8_find_first(fstring text, const ByteTR& tr) override {
        return byte_find_first(text, tr);
    }
    PosLen utf8_find_first(fstring text, const byte_t* tr) override {
        return byte_find_first(text, tr);
    }
    PosLen byte_find_first(fstring text) override {
        return tpl_byte_find_first(text, IdentityTR());
    }
    PosLen byte_find_first(fstring text, const ByteTR& tr) override {
        return tpl_byte_find_first(text, tr);
    }
    PosLen byte_find_first(fstring text, const byte_t* tr) override {
        return tpl_byte_find_first(text, TableTranslator(tr));
    }

    /// implementation: find last by tpl_ac_scan_rev
    template<class TR>
    PosLen tpl_shortest_byte_find_first(fstring text, TR tr) {
        const AC* ac = static_cast<const AC*>(m_dfa);
        m_all_match.erase_all();
        m_regex_idvec.erase_all();
        PosLenRegexID first(-1, -1, -1);
        auto onHit = [&first,ac]
        (size_t beg, const WordID* pWords, size_t nWords, size_t) {
            auto  wid = pWords[nWords-1]; // shortest
            first.len = int(ac->wlen(wid));
            first.pos = int(beg);
            first.regex_id = int(wid);
        };
        ac->tpl_ac_scan_rev(text, onHit, tr);
        if (-1 == first.pos) {
            return PosLen(-1, -1);
        }
        else {
            m_regex_idvec.push_back(first.regex_id);
            return PosLen(first.pos, first.len);
        }
    }
    PosLen shortest_byte_find_first(fstring text) override {
        return tpl_shortest_byte_find_first(text, IdentityTR());
    }
    PosLen shortest_byte_find_first(fstring text, const ByteTR& tr) override {
        return tpl_shortest_byte_find_first(text, tr);
    }
    PosLen shortest_byte_find_first(fstring text, const byte_t* tr) override {
        return tpl_shortest_byte_find_first(text, TableTranslator(tr));
    }
    PosLen shortest_utf8_find_first(fstring text) override {
        return shortest_byte_find_first(text);
    }
    PosLen shortest_utf8_find_first(fstring text, const ByteTR& tr) override {
        return shortest_byte_find_first(text, tr);
    }
    PosLen shortest_utf8_find_first(fstring text, const byte_t* tr) override {
        return shortest_byte_find_first(text, tr);
    }

    template<class TR>
    size_t tpl_byte_find_all(fstring text, TR tr) {
        const AC* ac = static_cast<const AC*>(m_dfa);
        m_all_match.erase_all();
        m_regex_idvec.erase_all();
        auto onHit = [ac,this]
        (size_t beg, const WordID* pWords, size_t, size_t) {
            auto wid = pWords[0]; // longest
            int  len = ac->wlen(wid);
            this->m_all_match.emplace_back(int(beg), len, wid);
        };
        ac->tpl_ac_scan_rev(text, onHit, tr);
        std::reverse(m_all_match.begin(), m_all_match.end());
        return m_all_match.size();
    }
	size_t byte_find_all(fstring text) override {
        return tpl_byte_find_all(text, IdentityTR());
    }
	size_t byte_find_all(fstring text, const ByteTR& tr) override {
        return tpl_byte_find_all(text, tr);
    }
	size_t byte_find_all(fstring text, const byte_t* tr) override {
        return tpl_byte_find_all(text, TableTranslator(tr));
    }
    size_t utf8_find_all(fstring text) override {
        return byte_find_all(text);
    }
    size_t utf8_find_all(fstring text, const ByteTR& tr) override {
        return byte_find_all(text, tr);
    }
    size_t utf8_find_all(fstring text, const byte_t* tr) override {
        return byte_find_all(text, tr);
    }

    template<class TR>
    size_t tpl_shortest_byte_find_all(fstring text, TR tr) {
        const AC* ac = static_cast<const AC*>(m_dfa);
        m_all_match.erase_all();
        m_regex_idvec.erase_all();
        auto onHit = [ac,this]
        (size_t beg, const WordID* pWords, size_t nWords, size_t) {
            auto wid = pWords[nWords-1]; // shortest
            int  len = ac->wlen(wid);
            this->m_all_match.emplace_back(int(beg), len, wid);
        };
        ac->tpl_ac_scan_rev(text, onHit, tr);
        std::reverse(m_all_match.begin(), m_all_match.end());
        return m_all_match.size();
    }
	size_t shortest_byte_find_all(fstring text) override {
        return tpl_shortest_byte_find_all(text, IdentityTR());
    }
	size_t shortest_byte_find_all(fstring text, const ByteTR& tr) override {
        return tpl_shortest_byte_find_all(text, tr);
    }
	size_t shortest_byte_find_all(fstring text, const byte_t* tr) override {
        return tpl_shortest_byte_find_all(text, TableTranslator(tr));
    }
	size_t shortest_utf8_find_all(fstring text) override {
        return shortest_byte_find_all(text);
    }
	size_t shortest_utf8_find_all(fstring text, const ByteTR& tr) override {
        return shortest_byte_find_all(text, tr);
    }
	size_t shortest_utf8_find_all(fstring text, const byte_t* tr) override {
        return shortest_byte_find_all(text, tr);
    }

    template<class TR>
    size_t tpl_byte_find_all_len(fstring text, TR tr) {
        const AC* ac = static_cast<const AC*>(m_dfa);
        m_all_match.erase_all();
        m_regex_idvec.erase_all();
        MRE_AC_OnHitAll onHit{&m_all_match, ac};
        ac->tpl_ac_scan_rev(text, onHit, tr);
        std::reverse(m_all_match.begin(), m_all_match.end());
        return m_all_match.size();
    }
    size_t byte_find_all_len(fstring text) override {
        return tpl_byte_find_all_len(text, IdentityTR());
    }
    size_t byte_find_all_len(fstring text, const ByteTR& tr) override {
        return tpl_byte_find_all_len(text, tr);
    }
    size_t byte_find_all_len(fstring text, const byte_t* tr) override {
        return tpl_byte_find_all_len(text, TableTranslator(tr));
    }
    size_t utf8_find_all_len(fstring text) override {
        return byte_find_all_len(text);
    }
    size_t utf8_find_all_len(fstring text, const ByteTR& tr) override {
        return byte_find_all_len(text, tr);
    }
    size_t utf8_find_all_len(fstring text, const byte_t* tr) override {
        return byte_find_all_len(text, TableTranslator(tr));
    }
};

MultiRegexFullMatch* MultiRegexFullMatch_ac(const BaseDFA* dfa) {
#define TryAC(AC) \
	if (const AC* d = dynamic_cast<const AC*>(dfa)) \
		return new MultiRegexFullMatchAC<AC>(d)

    TryAC(Aho_Corasick_DoubleArray_8B);
    TryAC(Aho_Corasick_SmartDFA_AC_State32);
    TryAC(Aho_Corasick_SmartDFA_AC_State4B);
    return NULL;
}

} // namespace terark

