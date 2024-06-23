/// @param tr(byte_t c) translate c, such as 'tolower'
template<class Translator>
size_t
match_path(fstring str, valvec<state_id_t>* path, Translator tr) const {
	MatchContext ctx;
	return match_path<Translator>(ctx, str, path, tr);
}

template<class Translator>
size_t match_path(MatchContext& ctx, fstring str
				, valvec<state_id_t>* path, Translator tr) const {
	size_t curr = ctx.root;
	path->resize(0);
	if (this->m_zpath_states)
		for (size_t i = ctx.pos, j = ctx.zidx;; ++i) {
			if (is_pzip(curr)) {
				fstring zs = get_zpath_data(curr, &ctx);
				size_t n = std::min<size_t>(zs.size(), str.size()-i);
				for (; j < n; ++i, ++j) {
					path->push_back(curr);
					if ((byte_t)tr(str[i]) != zs[j]) return i;
				}
				j = 0;
			}
			assert(i <= str.size());
			path->push_back(curr);
			if (str.size() == i)
				return i;
			state_id_t next = state_move(curr, (byte_t)tr(str[i]));
			if (nil_state == next)
				return i;
			assert(next < total_states());
			ASSERT_isNotFree(next);
			curr = next;
		}
	else
		for(size_t i = ctx.pos;; ++i) {
			assert(!is_pzip(curr));
			path->push_back(curr);
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

/// @return pos <  len     : the mismatch pos
///         pos == len     : matched all
///         pos == len + 1 : matched state is not a final state
size_t first_mismatch_pos(const fstring str) const {
	MatchContext ctx;
	return first_mismatch_pos(ctx, str);
}

size_t first_mismatch_pos(MatchContext& ctx, const fstring str) const {
	size_t curr = ctx.root;
	if (this->m_zpath_states)
		for (size_t i = ctx.pos, j = ctx.zidx; i < str.size(); ++i) {
			if (is_pzip(curr)) {
				fstring zs = get_zpath_data(curr, &ctx);
				assert(zs.size() > 0);
				size_t m = std::min(str.size() - i, zs.size());
				do { // prefer do .. while for performance
					if (str.p[i] != zs.p[j])
						return i;
					++i; ++j;
				} while (j < m);
				if (str.size() == i)
					return is_term(curr) ? str.n : str.n + 1;
				j = 0;
			}
			state_id_t next = state_move(curr, (byte_t)str[i]);
			if (nil_state == next)
				return i;
			assert(next < total_states());
			ASSERT_isNotFree(next);
			curr = next;
		}
	else
		for(size_t i = ctx.pos; i < str.size(); ++i) {
			assert(!is_pzip(curr));
			size_t next = state_move(curr, (byte_t)str[i]);
			if (nil_state == next)
				return i;
			assert(next < total_states());
			ASSERT_isNotFree(next);
			curr = next;
		}
	return !is_pzip(curr) && is_term(curr) ? str.n : str.n + 1;
}

