
#if 0
///@{
///@brief a companion for step_key
///
/// a common usage is after calling step_key, then filter with suffix_cnt
/// then calling this function

size_t for_each_value(const MatchContext& ctx, const OnMatchKey& on_match)
const override final {
	return tpl_for_each_value(ctx, &on_match);
}

template<class OnMatch>
size_t tpl_for_each_value(const MatchContext& ctx, OnMatch* on_match) const {
	return tpl_for_each_value<OnMatch&>(ctx, *on_match);
}

template<class OnMatch>
size_t tpl_for_each_value(const MatchContext& ctx, OnMatch on_match) const {
	auto on_suffix = [&](size_t nth, fstring value, size_t) {
		const size_t keylen = ctx.pos; // be const
		on_match(keylen, nth, value);
	};
	ForEachWord_DFS<const MyType&, decltype(on_suffix)> vis(*this, on_suffix);
	vis.start(ctx.root, ctx.zidx);
	return vis.nth; // number of iterated values
}
///@}
#endif

#include "tpl_for_each_word.hpp"

/// @param prefix is the prefix of value, used for LazyUnionDFA
/// @param keylen keylen of OnMatchKey, prepend_for_each_word has no this param
size_t prepend_for_each_value
(size_t keylen,
	size_t base_nth, fstring prefix,
	size_t root, size_t zidx, const OnMatchKey& on_match)
	const override final {
	MatchContext ctx;
	NonRecursiveForEachWord forEachWord(&ctx);
	return forEachWord(*this, base_nth, prefix,
		root, zidx, ForEachOnMatchKey<>(keylen, on_match));
}

size_t prepend_for_each_word
(size_t base_nth, fstring prefix,
	size_t root, size_t zidx, const OnNthWord& on_match)
	const override final {
	MatchContext ctx;
	NonRecursiveForEachWord forEachWord(&ctx);
	return forEachWord(*this, base_nth, prefix,
		root, zidx, ForEachOnNthWord<>(on_match));
}

size_t prepend_for_each_word_ex
(size_t base_nth, fstring prefix,
	size_t root, size_t zidx, const OnNthWordEx& on_match)
	const override final {
	MatchContext ctx;
	NonRecursiveForEachWord forEachWord(&ctx);
	return forEachWord(*this, base_nth, prefix, root, zidx, &on_match);
}

