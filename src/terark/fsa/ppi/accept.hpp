public:
bool accept(const fstring str) const {
	return accept(str, IdentityTR());
}
template<class TR>
bool accept(const fstring str, TR* tr) const {
	return accept<TR&>(str, *tr);
}
template<class TR>
bool accept(const fstring str, TR tr) const {
	MatchContext ctx;
	return accept<TR>(ctx, str, tr);
}

template<class TR>
bool accept(MatchContext& ctx, fstring str, TR tr) const {
	size_t curr = ctx.root;
	for (size_t i = ctx.pos, j = ctx.zidx; ; ++i) {
		if (this->is_pzip(curr)) {
			fstring z = this->get_zpath_data(curr, &ctx);
			assert(z.size() > 0);
			if (i + z.size() > str.size())
				return false;
			do { // prefer do .. while for performance
				if ((byte_t)tr(str[i++]) != z[j++])
					return false;
			} while (j < z.size());
			j = 0;
		}
		assert(i <= str.size());
		if (str.size() == i)
			return this->is_term(curr);
		size_t next = this->state_move(curr, (byte_t)tr(str[i]));
		if (this->nil_state == next)
			return false;
		assert(next < this->total_states());
		ASSERT_isNotFree(next);
		curr = next;
	}
}
