size_t
suffix_cnt(MatchContext& ctx, fstring exact_prefix)
const override {
	return tpl_suffix_cnt(ctx, exact_prefix, IdentityTR());
}
size_t
suffix_cnt(MatchContext& ctx, fstring exact_prefix, const ByteTR& tr)
const override {
	return tpl_suffix_cnt<const ByteTR&>(ctx, exact_prefix, tr);
}
size_t
suffix_cnt(MatchContext& ctx, fstring exact_prefix, const byte_t* tr)
const override {
	return tpl_suffix_cnt(ctx, exact_prefix, TableTranslator(tr));
}

void path_suffix_cnt(MatchContext& ctx, fstring str, valvec<size_t>* cnt)
const override {
	this->tpl_path_suffix_cnt(ctx, str, cnt, IdentityTR());
}

void
path_suffix_cnt(MatchContext& ctx, fstring str, valvec<size_t>* cnt, const ByteTR& tr)
const override {
	this->tpl_path_suffix_cnt<const ByteTR&>(ctx, str, cnt, tr);
}

void
path_suffix_cnt(MatchContext& ctx, fstring str, valvec<size_t>* cnt, const byte_t* tr)
const override {
	assert(NULL != tr);
	this->tpl_path_suffix_cnt(ctx, str, cnt, TableTranslator(tr));
}

template<class TR>
size_t tpl_suffix_cnt(MatchContext& ctx, fstring str, TR tr) const {
	assert(is_compiled);
	size_t curr = ctx.root;
	size_t num = 0; // not required to initialize
	size_t i = ctx.pos;
	for(; ; ++i) {
		num = inline_suffix_cnt(curr);
		if (this->is_pzip(curr)) {
			fstring z = this->get_zpath_data(curr, &ctx);
			size_t nn = std::min(str.size()-i, z.size());
			for (size_t j = ctx.zidx; j < nn; ++j, ++i) {
				if ((byte_t)tr((byte_t)str.p[i]) != (byte_t)z.p[j]) {
					ctx.zidx = j;
					goto Done;
				}
			}
			ctx.zidx = 0;
		}
		if (str.size() == i) break;
		size_t next = this->state_move(curr, (byte_t)tr((byte_t)str.p[i]));
		if (nil_state == next) break;
		curr = next;
	}
Done:
	ctx.pos = i;
	ctx.root = curr;
	return num;
}

template<class TR>
void
tpl_path_suffix_cnt(MatchContext& ctx, fstring str, valvec<size_t>* cnt, TR tr)
const {
	assert(is_compiled);
	assert(ctx.pos <= str.size());
	cnt->resize_no_init(str.size()+1);
	size_t* pcnt = cnt->data();
	size_t  curr = ctx.root;
	size_t  i = ctx.pos;
	for (; ; ++i) {
		size_t num = inline_suffix_cnt(curr);
		if (this->is_pzip(curr)) {
			fstring z = this->get_zpath_data(curr, &ctx);
			size_t nn = std::min(str.size()-i, z.size());
			for (size_t j = 0; j < nn; ++j, ++i) {
				pcnt[i] = num;
				if ((byte_t)tr((byte_t)str.p[i]) != (byte_t)z.p[j]) {
					ctx.zidx = j;
					goto Done;
				}
			}
			ctx.zidx = 0;
		}
		pcnt[i] = num;
		if (str.size() == i) break;
		size_t next = this->state_move(curr, (byte_t)tr((byte_t)str.p[i]));
		if (nil_state == next) break;
		curr = next;
	}
Done:
	ctx.pos = i;
	ctx.root = curr;
	cnt->risk_set_size(i+1);
}


