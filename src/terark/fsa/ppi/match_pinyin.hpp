
///@param workq
///@returns number of matched values
size_t match_pinyin(auchar_t delim, const valvec<fstrvec>& pinyin
	, const OnNthWord& onHanZiWord
	, valvec<size_t> workq[2]
	) const override final {
	assert(0 == this->m_zpath_states);
	if (0 != this->m_zpath_states) {
		THROW_STD(invalid_argument, "DFA must not be path_zip'ed");
	}
	workq[0].erase_all();
	workq[1].erase_all();
	workq[0].push_back(initial_state);
	for (size_t i = 0; i < pinyin.size(); ++i) {
		const fstrvec& multipy = pinyin[i]; // one hanzi has multiple pinyin
		for (size_t qi = 0; qi < workq[0].size(); ++qi) {
			for (size_t j = 0; j < multipy.size(); ++j) {
				fstring singlepy = multipy[j];
				size_t curr = workq[0][qi];
				assert(curr != nil_state);
				for (size_t k = 0; k < singlepy.size(); ++k) {
					byte_t ch = singlepy[k];
					size_t next = this->state_move(curr, ch);
					if (next == nil_state)
						goto Fail;
					curr = next;
				}
				workq[1].push_back(curr);
			Fail:;
			}
		}
		std::sort(workq[1].begin(), workq[1].end());
		workq[1].trim(std::unique(workq[1].begin(), workq[1].end()));
		workq[0].erase_all();
		workq[0].swap(workq[1]);
	}
	MatchContext ctx;
	NonRecursiveForEachWord forEachWord(&ctx);
	size_t total_matched = 0;
	for (size_t qi = 0; qi < workq[0].size(); ++qi) {
		size_t root = workq[0][qi];
		size_t valroot = this->state_move(root, delim);
		if (nil_state != valroot) {
			size_t nth = forEachWord(*this, valroot, 0, ForEachOnNthWord<>(onHanZiWord));
			total_matched += nth;
		}
	}
	return total_matched;
}

