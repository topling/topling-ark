using MatchingDFA::match_key;
using MatchingDFA::match_key_l;

size_t match_key
(MatchContext& ctx, auchar_t delim, fstring str, const OnMatchKey& on_match)
const override final {
	if (delim < 256 && delim != m_kv_delim) {
		assert(m_is_dag);
		check_is_dag(BOOST_CURRENT_FUNCTION);
	}
	return tpl_match_key(ctx, delim, str, &on_match);
}

size_t match_key
(MatchContext& ctx, auchar_t delim, fstring str, const OnMatchKey& on_match, const ByteTR& tr)
const override final {
	if (delim < 256 && delim != m_kv_delim) {
		assert(m_is_dag);
		check_is_dag(BOOST_CURRENT_FUNCTION);
	}
	return tpl_match_key(ctx, delim, str, &on_match, &tr);
}

size_t match_key
(MatchContext& ctx, auchar_t delim, ByteInputRange& r, const OnMatchKey& on_match)
const override final {
	if (delim < 256 && delim != m_kv_delim) {
		assert(m_is_dag);
		check_is_dag(BOOST_CURRENT_FUNCTION);
	}
	return stream_match_key(ctx, delim, &r, &on_match);
}
size_t match_key
(MatchContext& ctx, auchar_t delim, ByteInputRange& r, const OnMatchKey& on_match, const ByteTR& tr)
const override final {
	if (delim < 256 && delim != m_kv_delim) {
		assert(m_is_dag);
		check_is_dag(BOOST_CURRENT_FUNCTION);
	}
	return stream_match_key(ctx, delim, &r, &on_match, &tr);
}

size_t match_key_l
(MatchContext& ctx, auchar_t delim, fstring str, const OnMatchKey& on_match)
const override final {
	if (delim < 256 && delim != m_kv_delim) {
		assert(m_is_dag);
		check_is_dag(BOOST_CURRENT_FUNCTION);
	}
	return tpl_match_key_l(ctx, delim, str, &on_match);
}

size_t match_key_l
(MatchContext& ctx, auchar_t delim, fstring str, const OnMatchKey& on_match, const ByteTR& tr)
const override final {
	if (delim < 256 && delim != m_kv_delim) {
		assert(m_is_dag);
		check_is_dag(BOOST_CURRENT_FUNCTION);
	}
	return tpl_match_key_l(ctx, delim, str, &on_match, &tr);
}

size_t match_key_l
(MatchContext& ctx, auchar_t delim, ByteInputRange& r, const OnMatchKey& on_match)
const override final {
	if (delim < 256 && delim != m_kv_delim) {
		assert(m_is_dag);
		check_is_dag(BOOST_CURRENT_FUNCTION);
	}
	return stream_match_key_l(ctx, delim, &r, &on_match);
}

size_t match_key_l
(MatchContext& ctx, auchar_t delim, ByteInputRange& r, const OnMatchKey& on_match, const ByteTR& tr)
const override final {
	if (delim < 256 && delim != m_kv_delim) {
		assert(m_is_dag);
		check_is_dag(BOOST_CURRENT_FUNCTION);
	}
	return stream_match_key_l(ctx, delim, &r, &on_match, &tr);
}

bool step_key(MatchContext& ctx, auchar_t delim, fstring str)
const override final {
	return tpl_step_key(ctx, delim, str, IdentityTR());
}
bool step_key(MatchContext& ctx, auchar_t delim, fstring str, const ByteTR& tr)
const override final {
	return tpl_step_key<const ByteTR&>(ctx, delim, str, tr);
}
bool step_key_l(MatchContext& ctx, auchar_t delim, fstring str)
const override final {
	return tpl_step_key_l(ctx, delim, str, IdentityTR());
}
bool step_key_l(MatchContext& ctx, auchar_t delim, fstring str, const ByteTR& tr)
const override final {
	return tpl_step_key_l<const ByteTR&>(ctx, delim, str, tr);
}

#include "tpl_match_key.hpp"
