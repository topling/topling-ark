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
public:
state_id_t create_min_realnum(fstring min) {
	if (min.empty())
		return nil_state;

	fstring int_part, frac_part;
	if (const char* dot_pos = min.strchr('.')) {
		int_part = fstring(min.begin(), dot_pos);
		frac_part = fstring(dot_pos + 1, min.end());
	} else {
		int_part = min;
		frac_part = "";
	}

	state_id_t root = this->new_state();

	// 用于“数值已经胜出”的通用终态（接受后续任意数字）
	state_id_t any_valid_suffix = this->_create_any_fraction_state();

	size_t n_len = int_part.length();

	// =========================================================
	// 路径 A: 整数位数更多 (Strictly Longer Integers)
	// =========================================================
	// 逻辑：如果输入整数位数 > min 的整数位数，则数值一定更大。
	// 例如 min="93"，我们接受 "100", "1000" 等。
	// 必须避免前导0 (例如 min="5", 不接受 "06")。

	state_id_t longer_curr = this->new_state();
	// 首位只能是 1-9 (除了 0 本身，但 0 的长度只有1，这里处理长度>1的情况，所以首位必定非0)
	this->add_range_move(root, longer_curr, '1', '9');

	// 既然首位已经消耗了1个长度，我们需要再消耗 n_len 个长度才能确保总长度 > n_len
	// 例如 min="5" (len=1). Root ->(1-9)-> S1. S1已经是长度1。
	// 我们需要长度 >= 2。所以 S1 ->(0-9)-> S2(终态)。总长度2。
	for (size_t i = 0; i < n_len; ++i) {
		state_id_t next = this->new_state();
		this->add_range_move(longer_curr, next, '0', '9');
		longer_curr = next;
	}

	// longer_curr 此时代表“整数部分长度已经超过 min”，它是合法的
	this->set_final(longer_curr);
	this->add_range_move(longer_curr, longer_curr, '0', '9'); // 整数可以更长
	this->add_move(longer_curr, any_valid_suffix, '.'); // 允许接小数

	// =========================================================
	// 准备工作：构建 exact_suffix 链
	// =========================================================
	// 当我们在“相同长度”比较中发现某一位更大时（例如 min=500, input=6..），
	// 我们不需要再比较数值，但必须强制要求剩余的整数位数正确，以保持位权。
	// exact_suffix[k] 表示：接受 k 个 [0-9] 后，整数结束，进入“胜出”状态。

	std::vector<state_id_t> exact_suffix(n_len + 1);

	// exact_suffix[0]: 整数部分刚好读完，且数值已胜出。
	exact_suffix[0] = this->new_state();
	this->set_final(exact_suffix[0]); // 既然数值胜出，整数结束即为接受
	this->add_move(exact_suffix[0], any_valid_suffix, '.'); // 允许任意小数

	for (size_t k = 1; k <= n_len; ++k) {
		exact_suffix[k] = this->new_state();
		this->add_range_move(exact_suffix[k], exact_suffix[k - 1], '0', '9');
	}

	// =========================================================
	// 路径 B: 整数位数相同 (Same Length Integers)
	// =========================================================
	state_id_t curr = root;

	for (size_t i = 0; i < n_len; ++i) {
		char d = int_part[i]; // min 在该位的数字

		// 1. 输入位 > min位 (胜出)
		// 跳转到 exact_suffix 链，消耗掉剩余的位数 (n_len - 1 - i)
		char start_ch = (char)(d + 1);

		// 特殊处理 i=0 时的前导0约束：
		// 如果 min="0" (i=0, d='0')，胜出范围 '1'-'9' (OK)。
		// 如果 min="50" (i=0, d='5')，胜出范围 '6'-'9' (OK)。
		// 如果 min 很大，i=0 时 start_ch 依然遵循 1-9 约束。
		// 由于 min 本身无前导0，如果 n_len > 1，则 min[0] >= '1'。
		// 所以 start_ch >= '2'，自然不包含 '0'。
		// 只有当 min="0" 时，d='0'，start_ch='1'。
		// 结论：直接使用 d+1 即可，不用特判前导0，因为 d+1 肯定 > 0。

		if (start_ch <= '9') {
			this->add_range_move(curr, exact_suffix[n_len - 1 - i], start_ch, '9');
		}

		// 2. 输入位 == min位 (继续)
		// 创建新节点继续下一位比较
		state_id_t next_comp = this->new_state();
		this->add_move(curr, next_comp, (unsigned char)d);
		curr = next_comp;
	}

	// 此时 curr 处于“整数部分完全匹配 min 的整数部分”的状态。

	// =========================================================
	// 路径 C: 小数部分比较 (Fractional Part)
	// =========================================================

	if (frac_part.empty()) {
		// Case 1: min 没有小数 (e.g., "93")
		// 此时输入 "93" (curr) 是合法的 (93 >= 93)
		this->set_final(curr);
		// 输入 "93.xxx" 也是合法的，且任何小数都比 "无小数" 大
		this->add_move(curr, any_valid_suffix, '.');
	}
	else {
		// Case 2: min 有小数 (e.g., "93.58")
		// 此时输入 "93" (curr) 是不合法的 (93 < 93.58)，所以 curr 不是终态。
		// 必须读取小数点
		state_id_t frac_start = this->new_state();
		this->add_move(curr, frac_start, '.');
		curr = frac_start;

		size_t f_len = frac_part.length();

		for (size_t j = 0; j < f_len; ++j) {
			char c = frac_part[j];

			// 1. 小数位 > min位 (胜出)
			// 小数没有位权长度限制，一旦某位更大，后面跟什么都行
			if (c < '9') {
				this->add_range_move(curr, any_valid_suffix, (char)(c + 1), '9');
			}

			// 2. 小数位 == min位 (继续)
			state_id_t next_f = this->new_state();
			this->add_move(curr, next_f, (unsigned char)c);
			curr = next_f;
		}

		// 循环结束，意味着小数部分完全匹配 (e.g. 输入 "93.58")
		// 这是合法的
		this->set_final(curr);

		// 如果输入还有更多小数位 (e.g. "93.581" > "93.58")，这也是合法的
		this->add_range_move(curr, any_valid_suffix, '0', '9');
	}

	return root;
}

template<class TmpDFA>
state_id_t create_min_max_realnum(fstring min, fstring max) {
	// 0. 基础校验
	// 这里假设 min 和 max 格式正确。若需比较 min > max，需解析字符串数值。
	// 为简化，假设外部调用者保证 min <= max (numerical)。
	// 若 min > max, 返回一个无终态的死节点
	if (min.empty() || max.empty())
		return nil_state;
	if (min[0] == '-' && max[0] == '-') {
		state_id_t neg_root = non_neg_min_max_realnum<TmpDFA>(max.substr(1), min.substr(1));
		state_id_t root = new_state();
		add_move(root, neg_root, '-');
		return root;
	}
	if (max[0] == '-') // invalid: min is positive but max is negtive
		return nil_state;
	if (min[0] == '-') {
		state_id_t root0max = non_neg_min_max_realnum<TmpDFA>("0", max);
		if (nil_state == root0max)
			return nil_state;
		state_id_t neg_root = non_neg_min_max_realnum<TmpDFA>("0", min.substr(1));
		if (nil_state == neg_root)
			return nil_state;
		state_id_t root = new_state();
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
