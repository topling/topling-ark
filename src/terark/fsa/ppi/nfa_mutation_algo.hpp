void add_path(state_id_t source, state_id_t target, auchar_t ch) {
	this->add_move(source, target, ch);
}
template<class Char>
void add_path(state_id_t source, state_id_t target, const basic_fstring<Char> str) {
	assert(source < this->total_states());
	assert(target < this->total_states());
	if (0 == str.n) {
		this->add_epsilon(source, target);
	} else {
		state_id_t curr = source;
		for (size_t i = 0; i < str.size()-1; ++i) {
			auchar_t ch = str.uch(i);
			assert(ch < this->get_sigma());
			state_id_t next = this->new_state();
			this->add_move(curr, next, ch);
			curr = next;
		}
		this->add_move(curr, target, str.end()[-1]);
	}
}

template<class Char>
state_id_t add_path(state_id_t source, const basic_fstring<Char> str) {
	TERARK_ASSERT_LT(source, this->total_states());
	state_id_t curr = source;
	for (size_t i = 0; i < str.size(); ++i) {
		auchar_t ch = str.uch(i);
		assert(ch < this->get_sigma());
		state_id_t next = this->new_state();
		this->add_move(curr, next, ch);
		curr = next;
	}
	return curr;
}

state_id_t add_word(const fstring word) {
	return add_word_aux(initial_state, word);
}
state_id_t add_word(state_id_t RootState, const fstring word) {
	return add_word_aux(RootState, word);
}
state_id_t add_word16(const fstring16 word) {
	return add_word_aux(initial_state, word);
}
state_id_t add_word16(state_id_t RootState, const fstring16 word) {
	return add_word_aux(RootState, word);
}
template<class Char>
state_id_t add_word_aux(state_id_t RootState, const basic_fstring<Char> word) {
	state_id_t tail = add_path(RootState, word);
	this->set_final(tail);
	return tail;
}

state_id_t new_final_state() {
	state_id_t s = this->new_state();
	this->set_final(s);
	return s;
}

void add_range_move(state_id_t src, state_id_t dst, byte_t lo, byte_t hi) {
	for (byte_t c = lo; c <= hi; ++c) {
		this->add_move(src, dst, c);
	}
}

template<class SrcNFA>
state_id_t fast_copy_nfa(const SrcNFA& src) {
	size_t root = this->total_states();
	this->resize_states(root + src.total_states());
	valvec<state_id_t> epsilon;
	valvec<CharTarget<size_t> > non_eps;
	for (size_t i = 0; i < src.total_states(); i++) {
		src.get_epsilon_targets(i, &epsilon);
		src.get_non_epsilon_targets(i, non_eps);
		for (state_id_t t : epsilon) {
			this->add_epsilon(root + i, root + t);
		}
		for (CharTarget<size_t> t : non_eps) {
			this->add_move(root + i, root + t.target, t.ch);
		}
		if (src.is_final(i)) {
			this->set_final(root + i);
		}
	}
	return root;
}
template<class SrcDFA>
state_id_t fast_copy_dfa(const SrcDFA& src) {
	size_t root = this->total_states();
	this->resize_states(root + src.total_states());
	valvec<CharTarget<size_t> > children;
	for (size_t i = 0; i < src.total_states(); i++) {
		src.get_all_move(i, &children);
		for (CharTarget<size_t> t : children) {
			this->add_move(root + i, root + t.target, t.ch);
		}
		if (src.is_term(i)) {
			this->set_final(root + i);
		}
	}
	return root;
}

state_id_t create_min_decimal(fstring min) {
    size_t len = min.length();
    state_id_t start_node = new_state();

    // ==========================================
    // 1. 构建共享后缀链 (Suffix Chain)
    // suf_states[k] 表示：从这里开始，必须精确接受 k 个 '0'-'9' 才能到达终态
    // 用于处理 "位数相同但当前位更大" 的情况
    // ==========================================
    std::vector<state_id_t> suf_states(len + 1);
    suf_states[0] = this->new_state();
    this->set_final(suf_states[0]); // 接受 0 个字符即为终态
    m_tmp_finalstates.push_back(suf_states[0]);

    for (size_t k = 1; k <= len; ++k) {
        suf_states[k] = new_state();
        add_range_move(suf_states[k], suf_states[k-1], '0', '9');
    }

    // ==========================================
    // 2. 分支 A：处理位数更多的情况 (Length > len)
    // 逻辑：首位 [1-9] -> 接着至少 len 个 [0-9] -> 之后任意个 [0-9]
    // ==========================================
    state_id_t long_path_start = this->new_state();

    // 如果 min 是 "0"，长度为 1。位数更多意味着长度 >= 2。
    // 如果 min 是 "100"，长度为 3。位数更多意味着长度 >= 4。
    // 路径结构： Start --(1-9)--> S1 --(0-9)--> S2 ... --(0-9)--> S_FinalLoop

    // 只有当 min 不是空字符串时才生成此路径 (min是整数所以非空)
    // 创建一条长度为 len 的链，用来消耗掉比 min 长度多出来的那些位数
    state_id_t curr = long_path_start;
    for (size_t i = 0; i < len; ++i) {
        state_id_t next = this->new_state();
        add_range_move(curr, next, '0', '9');
        curr = next;
    }
    // 链条末端是终态，并且可以自环（接受更更长的数字）
    this->set_final(curr);
    m_tmp_finalstates.push_back(curr);
    add_range_move(curr, curr, '0', '9');

    // 将 Start 连接到这个长数字逻辑的入口
    // 注意：长数字不能有前导0，所以第一步只能是 '1'-'9'
    add_range_move(start_node, long_path_start, '1', '9');

    // ==========================================
    // 3. 分支 B：处理位数相同的情况 (Length == len)
    // 逻辑：逐位比较
    // ==========================================
    state_id_t match_curr = start_node;

    for (size_t i = 0; i < len; ++i) {
        byte_t digit = min[i];
        size_t remaining_len = len - 1 - i;

        // 情况 B.1: 当前位数字 > digit
        // 如果当前位我们选了一个比 min[i] 大的数字 d
        // 那么剩下的 remaining_len 位可以是任意数字。
        // 我们直接转移到 suf_states[remaining_len]。
        if (digit < '9') {
            // 如果是首位，不能是前导0。但 min[0] 本身 >= '0' (若min="0") 或 '1'。
            // 只要 digit < '9'，那么 digit+1 一定是 '1'~'9' 之间的，不可能是 '0'。
            // 所以这里直接添加转移即可。
            add_range_move(match_curr, suf_states[remaining_len], digit + 1, '9');
        }

        // 情况 B.2: 当前位数字 == digit
        // 我们需要继续匹配下一位。
        state_id_t match_next = this->new_state();

        this->add_move(match_curr, match_next, digit);
        match_curr = match_next;
    }
    this->set_final(match_curr);
    m_tmp_finalstates.push_back(match_curr);

    return start_node;
}

state_id_t create_min_max_decimal(fstring min, fstring max) {
	// 1. 规范化输入
	// 移除 + 号
	if (min.empty() || max.empty()) {
		return nil_state;
	}
	bool min_neg = false, max_neg = false;
	if (min[0] == '+')
		min = min.substr(1);
	else if (min[0] == '-')
		min = min.substr(1), min_neg = true;

	if (max[0] == '+')
		max = max.substr(1);
	else if (max[0] == '-')
		max = max.substr(1), max_neg = true;

	if (min.empty() || max.empty()) {
		return nil_state;
	}

	for (byte_t c : min) {
		if (c < '0' || c > '9')
			return nil_state;
	}
	for (byte_t c : max) {
		if (c < '0' || c > '9')
			return nil_state;
	}

	if (compare_val(min, min_neg, max, max_neg) > 0) {
		return nil_state;
	}

	state_id_t final_root = this->new_state();

	// 2. 分情况讨论
	if (!min_neg && !max_neg) {
		// Case A: 正数 到 正数 [0, 100] 或 [5, 10]
		state_id_t pos_root = create_positive_range(min, max);
		this->add_epsilon(final_root, pos_root);
	}
	else if (min_neg && max_neg) {
		// Case B: 负数 到 负数 [-100, -5]
		// 等价于构造正数范围 [5, 100] 然后前面加 '-'
		std::swap(min, max); // '-' has been deleted, just swap

		state_id_t pos_root = create_positive_range(min, max);
		state_id_t neg_start = this->new_state();
		this->add_move(neg_start, pos_root, '-');
		this->add_epsilon(final_root, neg_start);
	}
	else if (min_neg && !max_neg) {
		// Case C: 负数 到 正数 [-50, 100]
		// 拆分为 [-50, -1] 和 [0, 100]

		// 负数部分：对应正数 [1, 50]
		fstring neg_limit = min; // '-' has been deleted, just assign
		// 这里为了安全，检查 neg_limit 是否为 "0"，如果是 "-0" (虽然题目说没有)，处理逻辑会变
		// 题目保证 "0" 只有 "0"，所以 min="-..." 肯定是负数
		state_id_t neg_part = create_positive_range("1", neg_limit);
		state_id_t neg_prefix = this->new_state();
		this->add_move(neg_prefix, neg_part, '-');
		this->add_epsilon(final_root, neg_prefix);

		// 正数部分：[0, max]
		state_id_t pos_part = create_positive_range("0", max);
		this->add_epsilon(final_root, pos_part);
	}

	return final_root;
}

static int compare_int_str(fstring min, fstring max) {
	if (min.empty() || max.empty()) {
		return -2;
	}
	bool min_neg = false, max_neg = false;
	if (min[0] == '+')
		min = min.substr(1);
	else if (min[0] == '-')
		min = min.substr(1), min_neg = true;

	if (max[0] == '+')
		max = max.substr(1);
	else if (max[0] == '-')
		max = max.substr(1), max_neg = true;

	if (min.empty() || max.empty()) {
		return -2;
	}

	for (byte_t c : min) {
		if (c < '0' || c > '9')
			return -2;
	}
	for (byte_t c : max) {
		if (c < '0' || c > '9')
			return -2;
	}

	return compare_val(min, min_neg, max, max_neg);
}

private:
// 辅助结构：用于比较整数字符串大小
static bool str_num_less(const fstring& a, const fstring& b) {
	if (a.length() != b.length()) return a.length() < b.length();
	return a < b;
}

static bool str_num_greater(const fstring& a, const fstring& b) {
	return str_num_less(b, a);
}

// 比较 min 和 max 的数值大小，如果 min > max，返回空接受状态
// 简单的字符串比较逻辑需要考虑符号
static int compare_val(fstring a, bool a_neg, fstring b, bool b_neg) {
	if (a_neg && !b_neg) return -1; // 负 < 正
	if (!a_neg && b_neg) return 1;  // 正 > 负
	if (!a_neg && !b_neg) return str_num_less(a, b) ? -1 : (a == b ? 0 : 1);
	// 都是负数：-5 vs -3 -> "5" > "3" -> 实际 -5 < -3
	return str_num_less(a, b) ? 1 : (a == b ? 0 : -1);
};

// 构造一条长为 len 的链，每一步接受 0-9，最后指向 final_state（如果是 -1 则创建新终态）
// 返回链的起始状态
state_id_t build_any(size_t len) {
	state_id_t start = this->new_state();
	state_id_t current = start;
	for (size_t i = 0; i < len; ++i) {
		state_id_t next = this->new_state();
		for (char c = '0'; c <= '9'; ++c) {
			this->add_move(current, next, c);
		}
		current = next;
	}
	set_final(current);
	m_tmp_finalstates.push_back(current);
	return start;
}

// 构造接受 >= min_s (长度必须为 len) 的 NFA
// 例如 >= "34"：
// 第一位 > '3' ('4'-'9') -> 接任意长度 len-1
// 第一位 == '3' -> 递归处理 >= "4"
state_id_t build_ge(fstring min_s) {
	state_id_t start = this->new_state();
	state_id_t current = start;
	size_t len = min_s.length();

	for (state_id_t i = 0; i < len; ++i) {
		char d = min_s[i];
		state_id_t rem_len = len - 1 - i;

		// 分支 1: 当前位大于 d，后面任意
		if (d < '9') {
			state_id_t any_next = build_any(rem_len);
			for (char c = d + 1; c <= '9'; ++c) {
				this->add_move(current, any_next, c);
			}
		}

		// 分支 2: 当前位等于 d，继续链式处理
		state_id_t next_node = this->new_state();
		this->add_move(current, next_node, d);
		current = next_node;
	}
	set_final(current); // 走完所有等于的路径，说明相等，接受
	m_tmp_finalstates.push_back(current);
	return start;
}

valvec<state_id_t> m_tmp_finalstates;

// 构造接受 <= max_s (长度必须为 len) 的 NFA
state_id_t build_le(fstring max_s) {
	state_id_t start = this->new_state();
	state_id_t current = start;
	size_t len = max_s.length();

	for (state_id_t i = 0; i < len; ++i) {
		char d = max_s[i];
		state_id_t rem_len = len - 1 - i;

		// 分支 1: 当前位小于 d，后面任意
		if (d > '0') {
			state_id_t any_next = build_any(rem_len);
			for (char c = '0'; c < d; ++c) {
				this->add_move(current, any_next, c);
			}
		}

		// 分支 2: 当前位等于 d，继续
		state_id_t next_node = this->new_state();
		this->add_move(current, next_node, d);
		current = next_node;
	}
	m_tmp_finalstates.push_back(current);
	set_final(current);
	return start;
}

// 构造接受区间 [min_s, max_s] 的 NFA，前提：len(min_s) == len(max_s)
state_id_t build_range_same_len(fstring min_s, fstring max_s) {
	if (str_num_greater(min_s, max_s))
		return new_state(); // 死状态

	state_id_t start = this->new_state();
	state_id_t current = start;
	size_t len = min_s.length();
	bool diverged = false;

	for (size_t i = 0; i < len; ++i) {
		char low = min_s[i];
		char high = max_s[i];

		if (low == high) {
			// 公共前缀，单路径延伸
			state_id_t next = this->new_state();
			this->add_move(current, next, low);
			current = next;
		} else {
			// 分叉点
			size_t rem_len = len - 1 - i;

			// 1. 下界分支：当前位 == low，后续 >= min_s[i+1...]
			state_id_t node_ge = build_ge(min_s.substr(i + 1));
			this->add_move(current, node_ge, low);

			// 2. 中间分支：low < 当前位 < high，后续任意
			if (high - low > 1) {
				state_id_t node_any = build_any(rem_len);
				for (char c = low + 1; c < high; ++c) {
					this->add_move(current, node_any, c);
				}
			}

			// 3. 上界分支：当前位 == high，后续 <= max_s[i+1...]
			state_id_t node_le = build_le(max_s.substr(i + 1));
			this->add_move(current, node_le, high);

			diverged = true;
			break; // 处理完分叉后，子状态已经接管，主循环结束
		}
	}

	if (!diverged) {
		// 如果一直没有分叉（即 min_s == max_s），current需要标记为终态
		set_final(current);
		m_tmp_finalstates.push_back(current);
	}
	return start;
}

// 构造正数区间 [min_s, max_s] 的 NFA
// 处理不同长度的情况，并确保不含前导0 (除非是 "0"，但此函数处理正整数，"0"由上层特殊处理或作为长度1处理)
state_id_t create_positive_range(fstring min_s, fstring max_s) {
	state_id_t root = this->new_state();

	// 预处理：去掉前面的 + 号（如果有）
	if (!min_s.empty() && min_s[0] == '+') min_s = min_s.substr(1);
	if (!max_s.empty() && max_s[0] == '+') max_s = max_s.substr(1);

	if (str_num_greater(min_s, max_s)) return root; // 空

	size_t min_len = min_s.length();
	size_t max_len = max_s.length();

	for (size_t len = min_len; len <= max_len; ++len) {
		std::string local_min, local_max;

		// 确定当前长度下的下界
		if (len == min_len)
			local_min.assign(min_s.data(), min_s.size());
		else {
			// 长度 > min_len，下界是 100...0
			local_min = "1";
			local_min.append(len - 1, '0');
		}

		// 确定当前长度下的上界
		if (len == max_len)
			local_max.assign(max_s.data(), max_s.size());
		else {
			// 长度 < max_len，上界是 999...9
			local_max = std::string(len, '9');
		}

		// 特殊情况：如果是单字符 "0"，local_min 可能是 "0"
		// 上面的逻辑对于 len=1, min="0" -> local_min="0" 是兼容的
		// 对于 len>1, local_min="10..." 保证了无前导0

		state_id_t sub_root = build_range_same_len(local_min, local_max);
		this->add_epsilon(root, sub_root);
	}
	return root;
}

// 辅助：创建一个“接受任意小数后缀”的状态
// 该状态接受 [0-9]* 并且是终态 (用于数值已经确立大于 min 的情况)
state_id_t _create_any_fraction_state() {
	state_id_t s = new_state();
	set_final(s);
	add_range_move(s, s, '0', '9');
	return s;
}
state_id_t create_min_realnum_ge_int(fstring int_part) {
	m_tmp_finalstates.erase_all(); // for create_min_decimal
	state_id_t root = this->create_min_decimal(int_part);
	state_id_t xdot = this->new_state();
	state_id_t tail = this->_create_any_fraction_state(); // [0-9]*
	this->add_range_move(xdot, tail, '0', '9'); // [0-9]+
	for (state_id_t s : m_tmp_finalstates) {
		this->add_move(s, xdot, '.'); // [.][0-9]+
	}
	return root;
}
public:
state_id_t create_min_realnum(fstring min) {
	if (min.empty())
		return nil_state;
	const char* dot_pos = min.strchr('.');
	if (!dot_pos)
		return create_min_realnum_ge_int(min);
	fstring	int_part(min.begin(), dot_pos),	frac_part(dot_pos + 1, min.end());
	state_id_t root = this->new_state();
	add_move(add_path(root, int_part), create_lexical_ge(frac_part), '.');
	minimal_sso<32> int_part_plus_1(int_part);
	increment_as_bignum_allow_carry(int_part_plus_1, '0', '9');
	add_epsilon(root, create_min_realnum_ge_int(int_part_plus_1));
	return root;
}

template<class TmpDFA>
state_id_t create_min_max_realnum(fstring min, fstring max) {
	if (min.empty() || max.empty())
		return nil_state;
	if (min[0] == '-' && max[0] == '-') {
		state_id_t root = new_state(); // root first, tolerate leak on fail
		state_id_t neg_root = non_neg_min_max_realnum<TmpDFA>(max.substr(1), min.substr(1));
		if (nil_state == neg_root)
			return nil_state;
		add_move(root, neg_root, '-');
		return root;
	}
	if (max[0] == '-') // invalid: min is positive but max is negtive
		return nil_state;
	if (min[0] == '-') {
		state_id_t root = new_state(); // root first, tolerate leak on fail
		state_id_t root0max = non_neg_min_max_realnum<TmpDFA>("0", max);
		if (nil_state == root0max)
			return nil_state;
		state_id_t neg_root = non_neg_min_max_realnum<TmpDFA>("0", min.substr(1));
		if (nil_state == neg_root)
			return nil_state;
		this->add_move(root, neg_root, '-');
		this->add_move(root, root0max, '+');
		this->add_epsilon(root, root0max);
		return root;
	}
	return non_neg_min_max_realnum<TmpDFA>(min, max);
}

template<class TmpDFA>
state_id_t non_neg_min_max_realnum(fstring min, fstring max) {
	// 解析长度，find_i 如果未找到则返回 fstring.size()
	fstring min_i = min.substr(0, min.find_i('.'));
	fstring max_i = max.substr(0, max.find_i('.'));
	if (min_i.length() > max_i.length()) {
		return nil_state;
	}
	auto get_dfa = [&](auto create) {
		MyType nfa;
		nfa.erase_all(); // 删除默认创建的 initial_state
		state_id_t root = create(nfa);
		TERARK_VERIFY_EQ(root, initial_state);
		TmpDFA dfa;
		size_t ps_size = nfa.make_dfa(&dfa, SIZE_MAX, SIZE_MAX);
		(void)ps_size;
		dfa.graph_dfa_minimize();
		return dfa;
	};
	TmpDFA le_max_dfa = get_dfa([&](MyType& nfa) { return nfa.create_max_realnum(max); });
	TmpDFA ge_min_dfa = get_dfa([&](MyType& nfa) { return nfa.create_min_realnum(min); });
	TmpDFA result_dfa = le_max_dfa & ge_min_dfa; // intersection
	state_id_t root = fast_copy_dfa(result_dfa);
	return root;
}

state_id_t create_max_realnum(fstring max) {
	fstring max_i = max.substr(0, max.find_i('.'));
	TERARK_VERIFY_GE(max_i.size(), 1);
	state_id_t root = this->new_state();
	state_id_t eq_int_tail = this->add_word(root, max_i);
	if (max_i.size() < max.size()) { // 有小数部分
		fstring frac_str = max.substr(max_i.size() + 1);
		state_id_t frac_root = create_lexical_le(frac_str);
		add_move(eq_int_tail, frac_root, '.');
	}
	if (max_i != "0") {
		TERARK_VERIFY_NE(max_i[0], '0'); // 无前导 0
		terark::minimal_sso<32> str_max_i(max_i);
		bool success = decrement_as_bignum(str_max_i, '0', '9');
		TERARK_VERIFY(success);
		remove_leading_zero_as_bignum(str_max_i, '0'); // "100" -> "099" -> "99"
		state_id_t frac_root = _create_any_fraction_state();
		m_tmp_finalstates.erase_all();
		state_id_t less_root = create_positive_range("0", str_max_i);
		// create_positive_range 会将它产生的 final states 追加到 m_tmp_finalstates
		for (state_id_t int_final : m_tmp_finalstates) {
			add_move(int_final, frac_root, '.');
		}
		add_epsilon(root, less_root);
	}
	return root;
}

// 识别所有字典序 ≤ max 的字符串（包括空串）
state_id_t create_lexical_le(fstring max) {
	state_id_t root = new_final_state(); // 接受空串
	if (size_t len = max.size()) {
		state_id_t curr = root, tail = _create_any_fraction_state();
		for (size_t i = 0; i < len; ++i) {
			TERARK_ASSERT_BE(max[i], '0', '9');
			state_id_t next = new_final_state();
			add_move(curr, next, max[i]);
			for (char c = '0', c_max = max[i]; c < c_max; ++c) {
				add_move(curr, tail, c);
			}
			curr = next;
		}
	}
	return root;
}

// 识别所有字典序 ≥ min 的字符串
state_id_t create_lexical_ge(fstring min) {
	size_t len = min.size();
	if (len == 0) { // min 为空：接受所有字符串（包括空串）
		return _create_any_fraction_state();
	}
	state_id_t root = new_state();
	state_id_t curr = root, tail = _create_any_fraction_state();
	for (size_t i = 0; i < len; ++i) {
		TERARK_ASSERT_BE(min[i], '0', '9');
		state_id_t next = (i < len - 1) ? new_state() : tail;
		add_move(curr, next, min[i]);
		for (char ch = min[i] + 1; ch <= '9'; ++ch) {
			add_move(curr, tail, ch);
		}
		curr = next;
	}
	return root;
}
