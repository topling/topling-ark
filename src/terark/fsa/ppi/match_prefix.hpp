template<class OnMatch>
size_t tpl_match_prefix(fstring str, OnMatch* on_match) const {
	return tpl_match_prefix<OnMatch&>(str, *on_match, IdentityTR());
}
template<class OnMatch, class TR>
size_t tpl_match_prefix(fstring str, OnMatch* on_match, TR* tr) const {
	return tpl_match_prefix<OnMatch&, TR&>(str, *on_match, *tr);
}

/// @param on_match(size_t endpos)
/// @return max forward scaned chars
template<class OnMatch>
size_t tpl_match_prefix(fstring str, OnMatch on_match) const {
	return tpl_match_prefix<OnMatch>(str, on_match, IdentityTR());
}

/// @param on_match(size_t endpos)
/// @param tr(byte_t c) translate c, such as 'tolower'
/// @return max forward scaned chars
template<class OnMatch, class Translator>
size_t tpl_match_prefix(fstring str, OnMatch on_match, Translator tr)
const {
	MatchContext ctx;
	return tpl_match_prefix<OnMatch, Translator>(ctx, str, on_match, tr);
}

#if !defined(TERARK_FSA_has_match_prefix_impl)
template<class OnMatch, class TR>
size_t
tpl_match_prefix(MatchContext& ctx, fstring str, OnMatch on_match, TR tr)
const {
	size_t curr = ctx.root;
	for (size_t i = ctx.pos, j = ctx.zidx; ; ++i) {
		if (is_pzip(curr)) {
			fstring z = get_zpath_data(curr, &ctx);
			size_t  n = std::min(z.size(), str.size()-i+j);
			for (; j < n; ++i, ++j)
				if ((byte_t)tr(str[i]) != z[j]) return i;
			if (n < z.size())
				return str.size();
			j = 0;
		}
		assert(i <= str.size());
		if (is_term(curr))
			on_match(i);
		if (str.size() == i)
			return i;
		state_id_t next = state_move(curr, (byte_t)tr(str[i]));
		if (nil_state == next)
			return i;
		assert(next < total_states());
		ASSERT_isNotFree(next);
		curr = next;
	}
	assert(0);
}
#endif

template<class Range, class OnMatch, class TR>
size_t
stream_match_prefix(Range* r, OnMatch* on_match, TR* tr)
const {
	return stream_match_prefix<Range&, OnMatch&, TR&>(*r, *on_match, *tr);
}
template<class Range, class OnMatch>
size_t stream_match_prefix(Range* r, OnMatch* on_match) const {
	return stream_match_prefix<Range&, OnMatch&, IdentityTR>
		(*r, *on_match, IdentityTR());
}

/// @param r requirements:
///    bool_t b =  r.empty();
///    byte_t c = *r; // read once, so InputRange is OK
///              ++r;
/// @param on_match(size_t endpos)
/// @param tr(byte_t c) translate c, such as 'tolower'
/// @return max forward scaned chars
template<class Range, class OnMatch, class Translator>
size_t
stream_match_prefix(Range r, OnMatch on_match, Translator tr)
const {
	MatchContext ctx;
	return stream_match_prefix<Range,OnMatch,Translator>(ctx, r, on_match, tr);
}

#if !defined(TERARK_FSA_has_match_prefix_impl)
template<class Range, class OnMatch, class TR>
size_t
stream_match_prefix(MatchContext& ctx, Range r, OnMatch on_match, TR tr)
const {
	size_t curr = ctx.root;
	for (size_t i = ctx.pos, j = ctx.zidx; ; ++i, ++r) {
		if (is_pzip(curr)) {
			fstring z = get_zpath_data(curr, &ctx);
			for (; j < z.size(); ++i, ++j, ++r)
				if (r.empty() || (byte_t)tr(*r) != z[j]) return i;
			j = 0;
		}
		if (is_term(curr))
			on_match(i);
		if (r.empty())
			return i;
		state_id_t next = state_move(curr, (byte_t)tr(*r));
		if (nil_state == next)
			return i;
		assert(next < total_states());
		ASSERT_isNotFree(next);
		curr = next;
	}
	assert(0);
}
#endif

template<class Range, class OnMatch>
size_t stream_match_prefix(Range r, OnMatch on_match) const {
	return stream_match_prefix<Range, OnMatch, IdentityTR>
		(r, on_match, IdentityTR());
}

private:
template<class T>
struct GetArg {
	T* t;
	explicit GetArg(T* x) : t(x) {}
	void operator()(const T& x) { *t = x; }
};
typedef GetArg<size_t> GetLen;
public:
size_t match_prefix_l(fstring text, size_t* max_len)
const override final {
	*max_len = 0;
	GetLen full_len(max_len);
	size_t plen = tpl_match_prefix<GetLen>(text, full_len);
	return plen;
}
size_t match_prefix_l(fstring text, size_t* max_len
		, const ByteTR& tr
		) const override final {
	*max_len = 0;
	GetLen full_len(max_len);
	size_t plen = tpl_match_prefix<GetLen, const ByteTR&>(text, full_len, tr);
	return plen;
}
size_t match_prefix_l(ByteInputRange& r, size_t* max_len)
const override final {
	*max_len = 0;
	GetLen full_len(max_len);
	size_t plen = stream_match_prefix<ByteInputRange&, GetLen>(r, full_len);
	return plen;
}
size_t match_prefix_l(ByteInputRange& r, size_t* max_len
		, const ByteTR& tr
		) const override final {
	*max_len = 0;
	GetLen full_len(max_len);
	size_t plen = stream_match_prefix
		<ByteInputRange&, GetLen, const ByteTR&>(r, full_len, tr);
	return plen;
}

size_t match_prefix(MatchContext& ctx, fstring text
		, const MatchingDFA::OnPrefix& on_prefix
		) const override final {
	return tpl_match_prefix<const MatchingDFA::OnPrefix&, IdentityTR>
		(ctx, text, on_prefix, IdentityTR());
}
size_t match_prefix(MatchContext& ctx, fstring text
		, const MatchingDFA::OnPrefix& on_prefix
		, const ByteTR& tr
		) const override final {
	return tpl_match_prefix<const MatchingDFA::OnPrefix&, const ByteTR&>
		(ctx, text, on_prefix, tr);
}
size_t match_prefix(MatchContext& ctx, ByteInputRange& r
		, const MatchingDFA::OnPrefix& on_prefix
		) const override final {
	return stream_match_prefix<ByteInputRange&,
							   const MatchingDFA::OnPrefix&,
							   IdentityTR>
		(ctx, r, on_prefix, IdentityTR());
}
size_t match_prefix(MatchContext& ctx, ByteInputRange& r
		, const MatchingDFA::OnPrefix& on_prefix
		, const ByteTR& tr
		) const override final {
	return stream_match_prefix<ByteInputRange&,
							   const MatchingDFA::OnPrefix&,
							   const ByteTR&>
		(ctx, r, on_prefix, tr);
}


