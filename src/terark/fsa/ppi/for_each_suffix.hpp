public:

size_t
for_each_suffix(MatchContext& ctx, fstring prefix, const OnNthWord& f)
const override final {
	assert(m_is_dag);
	check_is_dag(BOOST_CURRENT_FUNCTION);
	return tpl_for_each_suffix<DFA_OnWord3>(ctx, prefix, DFA_OnWord3(f));
}

template<class OnSuffix>
size_t tpl_for_each_suffix(fstring prefix, OnSuffix* f) const {
	MatchContext ctx;
	return tpl_for_each_suffix<OnSuffix&>(ctx, prefix, *f);
}

/// @param on_suffix(size_t nth, fstring suffix, state_id_t accept_state)
///    calling of on_nth_word is in lexical_graphical order,
///    nth is the ordinal, 0 based
/// @return number of suffixes
template<class OnSuffix>
size_t tpl_for_each_suffix(fstring prefix, OnSuffix on_suffix) const {
	MatchContext ctx;
	return tpl_for_each_suffix<OnSuffix>(ctx, prefix, on_suffix);
}
template<class OnSuffix>
size_t
tpl_for_each_suffix(MatchContext& ctx, fstring prefix, OnSuffix* on_suffix)
const {
	return tpl_for_each_suffix<OnSuffix&>(ctx, prefix, *on_suffix);
}
template<class OnSuffix>
size_t
tpl_for_each_suffix(MatchContext& ctx, fstring prefix, OnSuffix on_suffix)
const {
	assert(ctx.root < total_states());
	ASSERT_isNotFree(ctx.root);
	size_t curr = ctx.root;
	size_t prelen = 0;
	for (size_t i = ctx.pos; ; ++i) {
		if (is_pzip(curr)) {
			fstring zs = get_zpath_data(curr, &ctx);
			size_t  n = std::min<size_t>(zs.size(), prefix.n-i);
			for (size_t j = ctx.zidx; j < n; ++i, ++j)
				if (prefix.p[i] != zs.p[j]) {
					ctx.pos = i;
					ctx.root = curr;
					ctx.zidx = j;
					return 0;
				}
			prelen = n;
			ctx.zidx = 0;
		} else
			prelen = 0;
		assert(i <= prefix.size());
		if (prefix.size() == i)
			break;
		size_t next = state_move(curr, byte_t(prefix.p[i]));
		if (nil_state == next)
			return 0;
		assert(next < total_states());
		ASSERT_isNotFree(next);
		curr = next;
	}
	auto& forEachWord = NonRecursiveForEachWord::cache(ctx);
	return forEachWord(*this, curr, prelen, on_suffix);
}


