template<class OnMatch>
size_t tpl_match_key
(auchar_t delim, fstring str, OnMatch* on_match)
const {
	assert(delim < sigma);
	return tpl_match_key<OnMatch&, IdentityTR>(delim, str, *on_match, IdentityTR());
}
template<class OnMatch, class TR>
size_t tpl_match_key
(auchar_t delim, fstring str, OnMatch* on_match, TR* tr)
const {
	return tpl_match_key<OnMatch&, TR&>(delim, str, *on_match, *tr);
}
template<class OnMatch>
size_t tpl_match_key
(MatchContext& ctx, auchar_t delim, fstring str, OnMatch* on_match)
const {
	return tpl_match_key<OnMatch&, IdentityTR>(ctx, delim, str, *on_match, IdentityTR());
}template<class OnMatch, class TR>
size_t tpl_match_key
(MatchContext& ctx, auchar_t delim, fstring str, OnMatch* on_match, TR* tr)
const {
	return tpl_match_key<OnMatch&, TR&>(ctx, delim, str, *on_match, *tr);
}

template<class Range, class OnMatch>
size_t stream_match_key
(auchar_t delim, Range* r, OnMatch* on_match)
const {
	return stream_match_key<Range&, OnMatch&>(delim, *r, *on_match);
}
template<class Range, class OnMatch, class TR>
size_t stream_match_key
(auchar_t delim, Range* r, OnMatch* on_match, TR* tr)
const {
	MatchContext ctx;
	return stream_match_key<Range&, OnMatch&, TR&>(ctx, delim, *r, *on_match, *tr);
}
template<class Range, class OnMatch>
size_t stream_match_key
(MatchContext& ctx, auchar_t delim, Range* r, OnMatch* on_match)
const {
	return stream_match_key<Range&, OnMatch&, IdentityTR>
			(ctx, delim, *r, *on_match, IdentityTR());
}
template<class Range, class OnMatch, class TR>
size_t stream_match_key
(MatchContext& ctx, auchar_t delim, Range* r, OnMatch* on_match, TR* tr)
const {
	return stream_match_key<Range&, OnMatch&, TR&>(ctx, delim, *r, *on_match, *tr);
}

template<class OnMatch>
size_t tpl_match_key_l
(auchar_t delim, fstring str, OnMatch* on_match)
const {
	assert(delim < sigma);
	return tpl_match_key_l<OnMatch&>(delim, str, *on_match, IdentityTR());
}
template<class OnMatch, class TR>
size_t tpl_match_key_l
(auchar_t delim, fstring str, OnMatch* on_match, TR* tr)
const {
	assert(delim < sigma);
	return tpl_match_key_l<OnMatch&, TR&>(delim, str, *on_match, *tr);
}
template<class OnMatch>
size_t tpl_match_key_l
(MatchContext& ctx, auchar_t delim, fstring str, OnMatch* on_match)
const {
	assert(delim < sigma);
	return tpl_match_key_l<OnMatch&>(ctx, delim, str, *on_match, IdentityTR());
}
template<class OnMatch, class TR>
size_t tpl_match_key_l
(MatchContext& ctx, auchar_t delim, fstring str, OnMatch* on_match, TR* tr)
const {
	assert(delim < sigma);
	return tpl_match_key_l<OnMatch&, TR&>(ctx, delim, str, *on_match, *tr);
}

template<class Range, class OnMatch, class TR>
size_t stream_match_key_l
(auchar_t delim, Range* r, OnMatch* on_match, TR* tr)
const {
	return stream_match_key_l<Range&, OnMatch&, TR&>(delim, *r, *on_match, *tr);
}
template<class Range, class OnMatch>
size_t stream_match_key_l(auchar_t delim, Range* r, OnMatch* on_match) const {
	return stream_match_key_l<Range&, OnMatch&>(delim, *r, *on_match);
}

template<class Range, class OnMatch>
size_t stream_match_key_l
(MatchContext& ctx, auchar_t delim, Range* r, OnMatch* on_match)
const {
	return stream_match_key_l<Range&, OnMatch&, IdentityTR>(ctx, delim, *r, *on_match, IdentityTR());
}
template<class Range, class OnMatch, class TR>
size_t stream_match_key_l
(MatchContext& ctx, auchar_t delim, Range* r, OnMatch* on_match, TR* tr)
const {
	return stream_match_key_l<Range&, OnMatch&, TR&>(ctx, delim, *r, *on_match, *tr);
}

/// @param on_match(size_t keylen, size_t value_idx, fstring value)
/// @return max forward scaned chars
template<class OnMatch>
size_t tpl_match_key(auchar_t delim, fstring str, OnMatch on_match)
const {
	assert(delim < sigma);
	return tpl_match_key<OnMatch>(delim, str, on_match, IdentityTR());
}

/// @param on_match(size_t keylen, size_t value_idx, fstring value)
/// @param tr(byte_t c) translate c, such as 'tolower'
/// @return max forward scaned chars
template<class OnMatch, class TR>
size_t tpl_match_key(auchar_t delim, fstring str, OnMatch on_match, TR tr)
const {
	assert(delim < sigma);
	MatchContext ctx;
	return tpl_match_key<OnMatch, TR>(ctx, delim, str, on_match, tr);
}
template<class OnMatch, class TR>
size_t tpl_match_key
(MatchContext& ctx, auchar_t delim, fstring str, OnMatch on_match, TR tr)
const {
	assert(delim < sigma);
	state_id_t curr = ctx.root;
	ForEachOnMatchKey<OnMatch> onWord(-1, on_match);
	auto& forEachWord = NonRecursiveForEachWord::cache(ctx);
	StateMoveContext smbuf;
	for (size_t i = ctx.pos; ; ++i) {
		if (is_pzip(curr)) {
			fstring zstr = get_zpath_data(curr, &ctx);
			for(size_t j = ctx.zidx; j < zstr.size(); ++j, ++i) {
				byte_t dfa_byte = zstr[j];
				if (dfa_byte == delim) {
					onWord.keylen = i;
					forEachWord(*this, curr, j+1, &onWord);
				}
				if (str.size() == i || (byte_t)tr(byte_t(str[i])) != dfa_byte)
					return i;
			}
			ctx.zidx = 0;
		}
		state_id_t value_start = state_move_slow(curr, delim, smbuf);
		if (nil_state != value_start) {
			onWord.keylen = i;
			forEachWord(*this, value_start, 0, &onWord);
		}
		if (str.size() == i)
			return i;
		state_id_t next = state_move_fast(curr, (byte_t)tr(byte_t(str[i])), smbuf);
		if (nil_state == next)
			return i;
		assert(next < total_states());
		ASSERT_isNotFree(next);
		curr = next;
	}
	assert(0);
}

template<class Range, class OnMatch>
size_t stream_match_key(auchar_t delim, Range r, OnMatch on_match) const {
	assert(delim < sigma);
	return stream_match_key<Range, OnMatch>(delim, r, on_match, IdentityTR());
}
template<class Range, class OnMatch, class TR>
size_t stream_match_key(auchar_t delim, Range r, OnMatch on_match, TR tr)
const {
	MatchContext ctx;
	return stream_match_key<Range, OnMatch, TR>(ctx, delim, r, on_match, tr);
}
template<class Range, class OnMatch>
size_t stream_match_key
(MatchContext& ctx, auchar_t delim, Range r, OnMatch on_match)
const {
	return stream_match_key<Range, OnMatch, IdentityTR>
			(ctx, delim, r, on_match, IdentityTR());
}
/// @param r requirements:
///    bool_t b =  r.empty();
///    byte_t c = *r; // read once, so InputRange is OK
///              ++r;
/// @param on_match(size_t keylen, size_t value_idx, fstring value)
/// @param tr(byte_t c) translate c, such as 'tolower'
/// @return max forward scaned chars
template<class Range, class OnMatch, class TR>
size_t stream_match_key
(MatchContext& ctx, auchar_t delim, Range r, OnMatch on_match, TR tr)
const {
	assert(delim < sigma);
	state_id_t curr = ctx.root;
	ForEachOnMatchKey<OnMatch> onWord(-1, on_match);
	auto& forEachWord = NonRecursiveForEachWord::cache(ctx);
	StateMoveContext smbuf;
	for (size_t i = ctx.pos; ; ++i, ++r) {
		if (is_pzip(curr)) {
			fstring zstr = get_zpath_data(curr, &ctx);
			for(size_t j = ctx.zidx; j < zstr.size(); ++j, ++i, ++r) {
				byte_t dfa_byte = zstr[j];
				if (dfa_byte == delim) {
					onWord.keylen = i;
					forEachWord(*this, curr, j+1, &onWord);
				}
				if (r.empty() || (byte_t)tr(*r) != dfa_byte)
					return i;
			}
			ctx.zidx = 0;
		}
		state_id_t value_start = state_move_slow(curr, delim, smbuf);
		if (nil_state != value_start) {
			onWord.keylen = i;
			forEachWord(*this, value_start, 0, &onWord);
		}
		if (r.empty())
			return i;
		state_id_t next = state_move_fast(curr, (byte_t)tr(*r), smbuf);
		if (nil_state == next)
			return i;
		assert(next < total_states());
		ASSERT_isNotFree(next);
		curr = next;
	}
	assert(0);
}

template<class OnMatch>
size_t tpl_match_key_l(auchar_t delim, fstring str, OnMatch on_match)
const {
	assert(delim < sigma);
	return tpl_match_key_l<OnMatch>(delim, str, on_match, IdentityTR());
}

template<class OnMatch, class TR>
size_t tpl_match_key_l(auchar_t delim, fstring str, OnMatch on_match, TR tr)
const {
	assert(delim < sigma);
	MatchContext ctx;
	return tpl_match_key_l<OnMatch, TR>(ctx, delim, str, on_match, tr);
}

template<class OnMatch, class TR>
size_t tpl_match_key_l
(MatchContext& ctx, auchar_t delim, fstring str, OnMatch on_match, TR tr)
const {
	assert(delim < sigma);
	size_t curr = ctx.root;
	size_t last_state = nil_state;
	size_t last_i = 0, last_j = 0, i;
	StateMoveContext smbuf;
	for (i = ctx.pos; ; ++i) {
		if (is_pzip(curr)) {
			fstring zstr = get_zpath_data(curr, &ctx);
			for (size_t j = ctx.zidx; j < zstr.size(); ++j, ++i) {
				byte_t c2 = zstr[j];
				if (c2 == delim) {
					last_i = i;
					last_j = j + 1;
					last_state = curr;
					goto Done;
				}
				if (str.size() == i || (byte_t)tr(byte_t(str[i])) != c2) {
					goto Done;
				}
			}
			ctx.zidx = 0;
		}
		size_t value_start = state_move_slow(curr, delim, smbuf);
		if (nil_state != value_start) {
			last_i = i;
			last_j = 0;
			last_state = value_start;
		}
		if (str.size() == i) {
			goto Done;
		}
		size_t next = state_move_fast(curr, (byte_t)tr(byte_t(str[i])), smbuf);
		if (nil_state == next) {
			goto Done;
		}
		assert(next < total_states());
		ASSERT_isNotFree(next);
		curr = next;
	}
	assert(0);
Done:
	if (nil_state != last_state) {
		ForEachOnMatchKey<OnMatch> onWord(last_i, on_match);
		auto& forEachWord = NonRecursiveForEachWord::cache(ctx);
		forEachWord(*this, last_state, last_j, &onWord);
	}
	return i;
}

template<class Range, class OnMatch>
size_t stream_match_key_l(auchar_t delim, Range r, OnMatch on_match)
const {
   	assert(delim < sigma);
	MatchContext ctx;
	return stream_match_key_l<Range, OnMatch>(ctx, delim, r, on_match);
}
template<class Range, class OnMatch, class TR>
size_t stream_match_key_l(auchar_t delim, Range r, OnMatch on_match, TR tr)
const {
   	assert(delim < sigma);
	MatchContext ctx;
	return stream_match_key_l<Range, OnMatch, TR>(ctx, delim, r, on_match, tr);
}
template<class Range, class OnMatch>
size_t stream_match_key_l
(MatchContext& ctx, auchar_t delim, Range r, OnMatch on_match)
const {
	assert(delim < sigma);
	return stream_match_key_l<Range, OnMatch, IdentityTR>(ctx, delim, r, on_match, IdentityTR());
}
template<class Range, class OnMatch, class TR>
size_t stream_match_key_l
(MatchContext& ctx, auchar_t delim, Range r, OnMatch on_match, TR tr)
const {
	assert(delim < sigma);
	state_id_t curr = ctx.root;
	state_id_t last_state = nil_state;
	size_t last_i = 0, last_j = 0, i;
	StateMoveContext smbuf;
	for (i = ctx.pos; ; ++i, ++r) {
		if (is_pzip(curr)) {
			fstring zs = get_zpath_data(curr, &ctx);
			for (size_t j = ctx.zidx; j < zs.size(); ++j, ++r) {
				byte_t c2 = zs[j];
				if (c2 == delim) {
					i += j;
					last_i = i;
					last_j = j + 1;
					last_state = curr;
					goto Done;
				}
				if (r.empty() || (byte_t)tr(*r) != c2) {
					i += j;
					goto Done;
				}
			}
			i += zs.size();
			ctx.zidx = 0;
		}
		state_id_t value_start = state_move_slow(curr, delim, smbuf);
		if (nil_state != value_start) {
			last_i = i;
			last_j = 0;
			last_state = value_start;
		}
		if (r.empty()) {
			goto Done;
		}
		state_id_t next = state_move_fast(curr, (byte_t)tr(*r), smbuf);
		if (nil_state == next) {
			goto Done;
		}
		assert(next < total_states());
		ASSERT_isNotFree(next);
		curr = next;
	}
	assert(0);
Done:
	if (nil_state != last_state) {
		ForEachOnMatchKey<OnMatch> onWord(last_i, on_match);
		auto& forEachWord = NonRecursiveForEachWord::cache(ctx);
		forEachWord(*this, last_state, last_j, &onWord);
	}
	return i;
}

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

template<class TR>
bool
tpl_step_key(MatchContext& ctx, auchar_t delim, fstring str, TR tr)
const {
	size_t parent = ctx.root;
	size_t pos2 = ctx.pos;
	if (pos2 >= str.size()) {
		THROW_STD(invalid_argument,
				"strpos=%zd >= str.size=%zd", pos2, str.size());
	}
	size_t zidx2 = ctx.zidx;
	StateMoveContext smbuf;
	for(; ; ++pos2) {
		assert(pos2 <= str.size());
		if (is_pzip(parent)) {
			auto   zs = get_zpath_data(parent, &ctx);
			size_t nn = std::min(zs.size(), str.size() - pos2 + zidx2);
			for (; zidx2 < nn; ++zidx2, ++pos2) {
				uint08_t dfa_byte = zs[zidx2];
				uint08_t str_byte = tr(str[pos2]);
				if (delim == dfa_byte) {
				   	if (str_byte == dfa_byte) {
						ctx.root = parent;
						ctx.zidx = zidx2 + 1;
						ctx.pos  =  pos2 + 1;
						return true;
					} else {
						ctx.root = parent;
						ctx.zidx = zidx2;
						ctx.pos  =  pos2;
						return false;
					}
				}
				if (str_byte != dfa_byte) {
					ctx.root = parent;
					ctx.zidx = zidx2;
					ctx.pos = pos2;
					return false;
				}
			}
			if (nn < zs.size()) {
				assert(str.size() == pos2);
				ctx.root = parent;
				if (delim == zs[nn]) {
					ctx.zidx = zidx2 + 1;
					ctx.pos  =  pos2 + 1;
					return true;
				} else {
					ctx.zidx = zidx2;
					ctx.pos  =  pos2;
					return false;
				}
			}
		} else {
			assert(0 == zidx2);
		}
		size_t valroot = state_move_slow(parent, delim, smbuf);
		if (nil_state != valroot) {
			if (str.size() == pos2 || (uint08_t)tr(str[pos2]) == delim) {
				ctx.root = valroot;
				ctx.zidx = 0;
				ctx.pos = pos2 + 1;
				return true;
			}
		}
		else if (str.size() == pos2) {
			ctx.root = parent;
			ctx.zidx = zidx2;
			ctx.pos = pos2;
			return false;
		}
		size_t child = state_move_fast(parent, (uint08_t)tr(str[pos2]), smbuf);
		if (nil_state == child) {
			ctx.root = parent;
			ctx.zidx = zidx2;
			ctx.pos = pos2;
			return false;
		}
		parent = child;
		zidx2 = 0;
	}
	return false; // never get here...
}

template<class TR>
bool
tpl_step_key_l(MatchContext& ctx, auchar_t delim, fstring str, TR tr)
const {
	size_t parent = ctx.root;
	size_t pos0 = ctx.pos;
	size_t pos2 = ctx.pos;
	if (pos2 >= str.size()) {
		THROW_STD(invalid_argument,
				"strpos=%zd >= str.size=%zd", pos2, str.size());
	}
	size_t zidx2 = ctx.zidx;
	StateMoveContext smbuf;
	for(; ; ++pos2) {
		assert(pos2 <= str.size());
		if (is_pzip(parent)) {
			auto   zs = get_zpath_data(parent, &ctx);
			size_t nn = std::min(zs.size(), str.size() - pos2 + zidx2);
			for (; zidx2 < nn; ++zidx2, ++pos2) {
				uint08_t dfa_byte = zs[zidx2];
				uint08_t str_byte = tr(str[pos2]);
				if (str_byte == dfa_byte && delim == dfa_byte) {
					ctx.root = parent;
					ctx.zidx = zidx2 + 1;
					ctx.pos  =  pos2 + 1;
				}
				if (str_byte != dfa_byte) goto Done;
			}
			if (nn < zs.size()) {
				assert(str.size() == pos2);
				if (delim == zs[nn]) {
					ctx.root = parent;
					ctx.zidx = zidx2 + 1;
					ctx.pos  =  pos2 + 1;
					return true;
				}
			   	else goto Done;
			}
		} else {
			assert(0 == zidx2);
		}
		size_t valroot = state_move_slow(parent, delim, smbuf);
		if (nil_state != valroot) {
			if (str.size() == pos2) {
				ctx.root = valroot;
				ctx.zidx = 0;
				ctx.pos  = pos2 + 1;
				return true;
			}
		   	else if ((uint08_t)tr(str[pos2]) == delim) {
				ctx.root = valroot;
				ctx.zidx = 0;
				ctx.pos  = pos2 + 1;
			}
		}
		else if (str.size() == pos2) goto Done;
		size_t child = state_move_fast(parent, (uint08_t)tr(str[pos2]), smbuf);
		if (nil_state == child) goto Done;
		parent = child;
		zidx2 = 0;
	}
	return false; // never get here...
Done:
	if (pos0 < ctx.pos) {
		return true;
	} else {
		ctx.root = parent;
		ctx.zidx = zidx2;
		ctx.pos = pos2;
		return false;
	}
}
