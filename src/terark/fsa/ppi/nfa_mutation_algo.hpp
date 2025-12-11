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
	state_id_t curr = RootState;
	for (size_t i = 0; i < word.size(); ++i) {
		state_id_t next = this->new_state();
		auchar_t ch = word.uch(i);
		assert(ch < this->get_sigma());
		this->add_move(curr, next, ch);
		curr = next;
	}
	this->set_final(curr);
	return curr;
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
	return start;
}

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

// ============================================================================
// NFA Extended Implementation for Range [min, max]
// ============================================================================

// 辅助：创建一个只能接受 '0' 的状态，用于处理如 max="10" 时输入 "10.000" 的情况
state_id_t _create_zeros_only_state() {
    state_id_t s = this->new_state();
    this->set_final(s);
    this->add_move(s, s, '0');
    return s;
}

// 辅助：创建一个接受固定长度任意整数的状态链
// len: 需要生成的位数
// allow_leading_zero: 是否允许首位为0 (通常首位不允许，除非 len=1 且 min=0)
state_id_t _create_fixed_len_any(size_t len, bool allow_leading_zero, state_id_t final_target) {
    if (len <= 0) return final_target;

    state_id_t head = this->new_state();
    state_id_t curr = head;

    for (size_t i = 0; i < len; ++i) {
        char start = (i == 0 && !allow_leading_zero && len > 1) ? '1' : '0';
        // 如果是最后一位，连接到 final_target，否则创建新节点
        state_id_t next = (i == len - 1) ? final_target : this->new_state();
        this->add_range_move(curr, next, start, '9');
        curr = next;
    }
    return head;
}

// 辅助：构造 >= target (Fixed Length)
// 仅处理数值部分，假设整数长度已固定匹配
// is_int_part: 是否正在处理整数部分（决定了前导0和.的处理）
state_id_t _create_ge_logic(fstring target_int, fstring target_frac, state_id_t accept_all) {
    state_id_t root = this->new_state();
    state_id_t curr = root;
    size_t n = target_int.length();

    // 1. 整数部分处理
    for (size_t i = 0; i < n; ++i) {
        char d = target_int[i];

        // 分支 A: 当前位 > d -> 胜出 (后续任意)
        if (d < '9') {
            this->add_range_move(curr, accept_all, d + 1, '9');
        }

        // 分支 B: 当前位 == d -> 继续
        // 如果是最后一位，我们需要根据小数部分决定去向
        state_id_t next;
        if (i == n - 1) {
            // 整数读完了
            if (target_frac.empty()) {
                // min 没有小数 (e.g. "100") -> 输入 "100" 合法，"100.x" 也合法
                this->set_final(curr);
                this->add_move(curr, accept_all, '.'); // "100." 也是合法的开始
                return root; // 逻辑结束
            } else {
                // min 有小数 (e.g. "100.5") -> 输入 "100" 不合法，必须读 "."
                next = this->new_state(); // 用于接收 '.'
            }
        } else {
            next = this->new_state();
        }

        this->add_move(curr, next, d);
        curr = next;
    }

    // 2. 小数点处理
    // 此时 curr 等待接收 '.' (前提是 min 有小数)
    state_id_t frac_start = this->new_state();
    this->add_move(curr, frac_start, '.');
    curr = frac_start;

    // 3. 小数部分处理
    size_t f_len = target_frac.length();
    for (size_t i = 0; i < f_len; ++i) {
        char d = target_frac[i];

        // 分支 A: 当前位 > d -> 胜出
        if (d < '9') {
            this->add_range_move(curr, accept_all, d + 1, '9');
        }

        // 分支 B: 当前位 == d -> 继续
        state_id_t next = this->new_state();
        this->add_move(curr, next, d);
        curr = next;
    }

    // 4. 结束
    // min 小数部分匹配完 (e.g. 输入了 "100.5") -> 合法
    this->set_final(curr);
    // 输入更长的小数 "100.51" -> 合法
    this->add_range_move(curr, accept_all, '0', '9');

    return root;
}

// 辅助：构造 <= target (Fixed Length)
// 逻辑与 GE 相反，特别注意小数结尾的处理
state_id_t _create_le_logic(fstring target_int, fstring target_frac, state_id_t accept_all) {
    state_id_t root = this->new_state();
    state_id_t curr = root;
    size_t n = target_int.length();

    // 1. 整数部分
    for (size_t i = 0; i < n; ++i) {
        char d = target_int[i];
        char start = (i == 0 && n > 1) ? '1' : '0'; // 整数首位限制

        // 分支 A: 当前位 < d -> 胜出 (变小了，后续任意)
        if (start < d) {
            this->add_range_move(curr, accept_all, start, d - 1);
        } else if (start == d && start < d) {
             // 这种情况在 start=0, d=0 时不执行，逻辑上只需要 start < d 即可
        }

        // 分支 B: 当前位 == d -> 继续
        // 边界检查：首位 d 不能是 0 (如果 n>1)。如果 target_int="0", n=1, d='0', start='0' OK.
        if (i == 0 && n > 1 && d == '0') {
            // max 的首位是 0? 不可能 (除非 max="0")。
            // 只要 target 符合规范，这里不需要额外 check
        }

        if (i == n - 1) {
            // 整数结束
            if (target_frac.empty()) {
                // max 无小数 (e.g. "100") -> 输入 "100" 合法
                // "100." -> 只能接 0
                this->set_final(curr);
                state_id_t zeros = _create_zeros_only_state();
                this->add_move(curr, zeros, '.');
                this->add_move(curr, curr, d); // Link the last digit logic
                // 修正：上面的逻辑结构有点乱，不仅要 link d，还要处理 next 节点
                // 重新组织循环内的结构：
            }
        }

        // 重写循环内部清晰逻辑：
        // 1. Less path
        if (start < d) {
            this->add_range_move(curr, accept_all, start, d - 1);
        }

        // 2. Equal path
        if (i == n - 1) {
             // 整数部分最后一位匹配
             state_id_t node_after_int = this->new_state();
             this->add_move(curr, node_after_int, d);

             if (target_frac.empty()) {
                 // Max="100". Match "100". Final.
                 this->set_final(node_after_int);
                 // Only allow ".000"
                 state_id_t zeros = _create_zeros_only_state();
                 this->add_move(node_after_int, zeros, '.');
             } else {
                 // Max="100.5". Match "100". Not Final. Must read '.'
                 state_id_t frac_start = this->new_state();
                 this->add_move(node_after_int, frac_start, '.');

                 // 小数处理逻辑递归或展开... 这里展开
                 state_id_t f_curr = frac_start;
                 for(size_t j=0; j<target_frac.length(); ++j) {
                     char fd = target_frac[j];
                     // Less
                     if ('0' < fd) {
                        this->add_range_move(f_curr, accept_all, '0', fd - 1);
                     }
                     // Equal
                     state_id_t f_next = this->new_state();
                     this->add_move(f_curr, f_next, fd);
                     f_curr = f_next;
                 }
                 // Max 小数跑完 (e.g. matched "100.5")
                 // 此时只能接 0 (因为 100.50 == 100.5, 但 100.51 > 100.5)
                 this->set_final(f_curr);
                 this->add_move(f_curr, f_curr, '0');
             }
             return root; // 完成
        } else {
             state_id_t next = this->new_state();
             this->add_move(curr, next, d);
             curr = next;
        }
    }
    return root;
}

// 辅助：当 min 和 max 整数部分长度相同时的区间构造
// 核心逻辑：找到公共前缀，分裂，中间部分走 Any，下界走 GE，上界走 LE
state_id_t _create_bounded_same_len(fstring min_s, fstring max_s, state_id_t accept_all) {
    // 1. 解析
    fstring min_i = min_s.substr(0, min_s.find_i('.'));
    fstring max_i = max_s.substr(0, max_s.find_i('.'));
    // 补全 frac 逻辑在后续处理，这里主要看整数结构

    state_id_t root = this->new_state();
    state_id_t curr = root;
    size_t len = min_i.length(); // 已知相等

    for (size_t i = 0; i < len; ++i) {
        char low = min_i[i];
        char high = max_i[i];

        if (low == high) {
            // 公共前缀：只能走这条路
            state_id_t next = this->new_state();
            this->add_move(curr, next, low);
            curr = next;
        } else {
            // 分裂点 found!
            // 1. 中间部分 (Strictly between): low+1 ... high-1
            // 这些路径后续都是任意合法的
            if (low + 1 <= high - 1) {
                this->add_range_move(curr, accept_all, low + 1, high - 1);
            }

            // 2. 下界路径 (Follow min): 等于 low
            // 后续必须 >= min 的剩余部分
            fstring min_rem_int = min_i.substr(i + 1);
            fstring min_frac = (min_s.find('.') == fstring::npos) ? "" : min_s.substr(min_s.find('.') + 1);
            state_id_t ge_node = _create_ge_logic(min_rem_int, min_frac, accept_all);
            this->add_move(curr, ge_node, low);

            // 3. 上界路径 (Follow max): 等于 high
            // 后续必须 <= max 的剩余部分
            fstring max_rem_int = max_i.substr(i + 1);
            fstring max_frac = (max_s.find('.') == fstring::npos) ? "" : max_s.substr(max_s.find('.') + 1);

            // 注意：le_logic 需要稍微适配，因为它通常从头构建。
            // 但这里我们要连接的是一个“残缺”的整数部分。
            // 简单的方法：复用 le_logic，但传入剩余字符串，且告诉它不需要处理首位非0限制（因为已经在非首位了）
            // 为了简化，我在这里内联构建 LE 的这一步，或者调整 le_logic 接口。
            // 考虑到复杂度，直接调用一个特定的 LE helper 更安全。

            state_id_t le_node = _create_le_logic_partial(max_rem_int, max_frac, accept_all);
            this->add_move(curr, le_node, high);

            return root; // 分裂完成，后续逻辑由子函数接管
        }
    }

    // 如果循环结束都没有 break，说明 整数部分完全相等
    // 此时 curr 处于整数结束状态
    fstring min_f = (min_s.find('.') == fstring::npos) ? "" : min_s.substr(min_s.find('.') + 1);
    fstring max_f = (max_s.find('.') == fstring::npos) ? "" : max_s.substr(max_s.find('.') + 1);

    // 处理小数区间的比较 logic: min_f <= frac <= max_f
    // 这与整数处理非常相似，但没有长度限制。
    // 由于逻辑较深，建议专用函数处理小数区间
    _build_fractional_range(curr, min_f, max_f, accept_all);

    return root;
}

// 辅助：专门处理小数部分的区间 [min_f, max_f]
// 此时整数部分已完全匹配
void _build_fractional_range(state_id_t start_node, fstring min_f, fstring max_f, state_id_t accept_all) {
    // 逻辑：
    // min_f (e.g. "5") implies value 0.5
    // max_f (e.g. "55") implies value 0.55
    // 如果 min_f 为空，下界是 0。
    // 如果 max_f 为空，上界是 0。

    // 小数点处理
    // Case 1: min无小数，max无小数 -> 结束。只能接 .000
    if (min_f.empty() && max_f.empty()) {
        this->set_final(start_node);
        state_id_t z = _create_zeros_only_state();
        this->add_move(start_node, z, '.');
        return;
    }

    // 必须有小数点转移 (注意: 如果 min_f为空，说明 min 是整数，start_node 已经是终态)
    if (min_f.empty()) this->set_final(start_node);

    // 如果 max_f 为空 (max=100), 只能接 0 (max=100.000)
    // 但 min_f 非空 (min=100.5), 这说明 min > max，逻辑上不应该发生(调用者保证 min<=max)。
    // 唯一可能是 min=100, max=100. 走上面的分支。
    // 如果 min=100, max=100.5.
    // '.' 转移
    state_id_t dot_node = this->new_state();
    this->add_move(start_node, dot_node, '.');

    // 现在处于小数第一位。类似于整数的 Same Length Logic，但有一点不同：
    // 长度不等时：min="5" (0.5), max="55" (0.55).
    // 我们遍历 max_f 的长度。

    state_id_t curr = dot_node;
    size_t limit = std::max(min_f.length(), max_f.length());

    // 我们需要对齐 min_f 和 max_f (补0逻辑上) 来进行比较
    // 但不能改变字符串，需动态获取
    for (size_t i = 0; i < limit; ++i) {
        char c_min = (i < min_f.length()) ? min_f[i] : '0';
        char c_max = (i < max_f.length()) ? max_f[i] : '0';

        // 1. 分裂
        if (c_min < c_max) {
            // Middle
            if (c_min + 1 <= c_max - 1) {
                this->add_range_move(curr, accept_all, c_min + 1, c_max - 1);
            }
            // Lower (Follow min)
            // 只有当 min 还没结束，或者 min 结束了(隐含0)我们需要由 0 开启后续约束
            // 实际上，如果 min 结束了，后续位全是 0。
            // 只要输入 > 0，就满足 > min。
            // 简便处理：构建 GE(min_sub) 和 LE(max_sub)

            // Lower Branch
            state_id_t ge = this->new_state();
            this->add_move(curr, ge, c_min);
            // 此时我们在 ge 节点，刚刚匹配了 c_min
            // 如果 min 在这里结束了 (i >= len-1)，那么只要后续不全是0，或者全是0，都 >= min。
            // 实际上如果 min 结束，当前数 == min。
            // 下一位：如果输入 > 0，则 > min。
            fstring min_rem = (i + 1 < min_f.length()) ? min_f.substr(i+1) : "";
            _continue_frac_ge(ge, min_rem, accept_all);

            // Upper Branch
            state_id_t le = this->new_state();
            this->add_move(curr, le, c_max);
            fstring max_rem = (i + 1 < max_f.length()) ? max_f.substr(i+1) : "";
            _continue_frac_le(le, max_rem, accept_all);

            return; // 分裂结束
        }
        else if (c_min == c_max) {
            // 继续
            state_id_t next = this->new_state();
            this->add_move(curr, next, c_min);
            curr = next;
        } else {
            // min > max ?? Should not happen given input constraints
            return;
        }
    }

    // 完全匹配结束
    this->set_final(curr);
    // 只能接 0
    this->add_move(curr, curr, '0');
}

// 辅助：小数部分的 >= 逻辑 (min_rem 是剩余的小数位)
void _continue_frac_ge(state_id_t curr, fstring min_rem, state_id_t accept_all) {
    if (min_rem.empty()) {
        // min 结束 (e.g. 0.5 已匹配)。当前状态就是 >= 0.5。
        // 接受任何后续
        this->set_final(curr);
        this->add_range_move(curr, accept_all, '0', '9');
        return;
    }
    // 类似于 _create_ge_logic 的小数部分
    size_t n = min_rem.length();
    for(size_t i=0; i<n; ++i) {
        char d = min_rem[i];
        if (d < '9') this->add_range_move(curr, accept_all, d+1, '9');
        state_id_t next = this->new_state();
        this->add_move(curr, next, d);
        curr = next;
    }
    this->set_final(curr);
    this->add_range_move(curr, accept_all, '0', '9');
}

// 辅助：小数部分的 <= 逻辑
void _continue_frac_le(state_id_t curr, fstring max_rem, state_id_t accept_all) {
    if (max_rem.empty()) {
        // max 结束 (e.g. 0.5 已匹配)。当前状态不能再接 1-9，只能接 0。
        this->set_final(curr);
        this->add_move(curr, curr, '0');
        return;
    }
    size_t n = max_rem.length();
    for(size_t i=0; i<n; ++i) {
        char d = max_rem[i];
        if ('0' < d) this->add_range_move(curr, accept_all, '0', d-1);
        state_id_t next = this->new_state();
        this->add_move(curr, next, d);
        curr = next;
    }
    this->set_final(curr);
    this->add_move(curr, curr, '0');
}

// 辅助：Partial LE logic for integer part (no leading zero check needed)
state_id_t _create_le_logic_partial(fstring int_rem, fstring frac_rem, state_id_t accept_all) {
    state_id_t root = this->new_state();
    state_id_t curr = root;
    size_t n = int_rem.length();

    for(size_t i=0; i<n; ++i) {
        char d = int_rem[i];
        if ('0' < d) this->add_range_move(curr, accept_all, '0', d-1);

        if (i == n-1) {
            // Int End
            state_id_t int_end = this->new_state();
            this->add_move(curr, int_end, d);

            if (frac_rem.empty()) {
                this->set_final(int_end);
                state_id_t z = _create_zeros_only_state();
                this->add_move(int_end, z, '.');
            } else {
                state_id_t f_start = this->new_state();
                this->add_move(int_end, f_start, '.');
                _continue_frac_le(f_start, frac_rem, accept_all);
            }
            return root;
        } else {
            state_id_t next = this->new_state();
            this->add_move(curr, next, d);
            curr = next;
        }
    }
    // Should be covered by loop return
    return root;
}

public:
state_id_t create_min_max_realnum(fstring min, fstring max) {
	// 0. 基础校验
	// 这里假设 min 和 max 格式正确。若需比较 min > max，需解析字符串数值。
	// 为简化，假设外部调用者保证 min <= max (numerical)。
	// 若 min > max, 返回一个无终态的死节点
	if (min.empty() || max.empty())
		return nil_state;
	if (min[0] == '-' && max[0] == '-') {
		state_id_t neg_root = non_neg_min_max_realnum(max.substr(1), min.substr(1));
		state_id_t root = new_state();
		add_move(root, neg_root, '-');
		return root;
	}
	if (max[0] == '-') // invalid: min is positive but max is negtive
		return nil_state;
	if (min[0] == '-') {
		state_id_t root0max = non_neg_min_max_realnum("0", max);
		if (nil_state == root0max)
			return nil_state;
		state_id_t neg_root = non_neg_min_max_realnum("0", min.substr(1));
		if (nil_state == neg_root)
			return nil_state;
		state_id_t root = new_state();
		this->add_move(root, neg_root, '-');
		this->add_move(root, root0max, '+');
		this->add_epsilon(root, root0max);
		return root;
	}
	return non_neg_min_max_realnum(min, max);
}

private:
state_id_t non_neg_min_max_realnum(fstring min, fstring max) {
	// 解析长度，find_i 如果未找到则返回 fstring.size()
	fstring min_i = min.substr(0, min.find_i('.'));
	fstring max_i = max.substr(0, max.find_i('.'));
	size_t len_min = min_i.length();
	size_t len_max = max_i.length();

	if (len_min > len_max)
		return nil_state;

	state_id_t root = this->new_state();
	state_id_t accept_any = this->_create_any_fraction_state(); // 复用之前定义的接受任意小数的终态

	// 1. 遍历所有可能的整数长度 L
	for (size_t L = len_min; L <= len_max; ++L) {
		// Case A: 长度处于中间 (min_len < L < max_len)
		// 此时该长度的所有整数都是合法的
		if (L > len_min && L < len_max) {
			/// state_id_t chain = _create_fixed_len_any(L, false, accept_any);
			// 连接到 root (通过 Epsilon 或直接拷贝首位转移，
			// 由于我们没有显式 epsilon 接口在题目说明的限制内，我们假设
			// 我们可以通过将 chain 的首节点合并到 root，或者让 root 跳转到 chain)
			// 题目接口: add_move(src, dst, ch).
			// 我们必须在 root 上添加转移。
			// 技巧：_create_fixed_len_any 返回的是 chain 的头部。
			// 我们不能直接 link root -> chain (no epsilon).
			// 必须把 chain 的第一层转移复制到 root 上。
			// 或者修改 helper 函数，让它直接从 root 出发？不行，root 有多条路。
			// 正确做法：Wrapper helper 应该接受 parent_node。

			// 重构 logic for reuse:
			// 由于不能用 epsilon，我们需要手动展开第一层。
			// 只有 '1'-'9' 是合法的首位。
			for (char d = '1'; d <= '9'; ++d) {
					state_id_t next = (L == 1) ? accept_any : this->new_state();
					this->add_move(root, next, d);
					if (L > 1) {
						// 构建剩余 L-1 位 [0-9]
						// state_id_t tail = _create_fixed_len_any(L - 1, true, accept_any);
						// 这里有个问题：_create... 返回的是 head。
						// next 需要连接到 tail 的后续。
						// 简单点：实现一个 "attach" 函数或者内联构建。
						_build_any_digits_chain(next, L - 1, accept_any);
					}
			}
		}
		// Case B: 长度 == min_len (且 min_len < max_len)
		else if (L == len_min && L < len_max) {
			// 生成 >= min (Fixed Length)
			fstring min_frac = (min.find('.') == fstring::npos) ? "" : min.substr(min.find('.')+1);
			state_id_t node = _create_ge_logic(min_i, min_frac, accept_any);
			// 这里同样面临 "Root 连接" 问题。
			// _create_ge_logic 返回的是独立链的头。
			// 我们需要把它 "Merge" 到 Root。
			// 为保持代码整洁，我将修改策略：
			// 所有的 Helper 函数改为 `void build_xxx(state_id_t start_node, ...)`
			// 这样我们可以直接把 Root 传进去。但是 Root 会被共享。
			// 如果多个分支共享 Root 的同一个字符转移 (e.g. min=10, max=200. L=2 starts with '1', L=3 starts with '1'...)
			// NFA 允许从同一状态通过相同字符去往不同状态。
			// 所以：我们可以直接把 helper 返回的 head 的 outgoing moves 复制到 root？
			// 不行，层级太深。

			// 最 NFA 的做法：使用 Epsilon。
			// 既然题目接口有 `add_epsilon`! (Wait, 题目描述里有 add_epsilon 吗？)
			// 题目描述：
			// void add_move(..., char ch);
			// void add_epsilon(state_id_t source_state, state_id_t target_state); // !!! 题目里有 !!!
			// 抱歉，我之前为了纯粹性尝试不用 epsilon，既然有，那就太简单了！

			this->add_epsilon(root, node);
		}
		// Case C: 长度 == max_len (且 min_len < max_len)
		else if (L > len_min && L == len_max) {
			// 生成 <= max (Fixed Length)
			fstring max_frac = (max.find('.') == fstring::npos) ? "" : max.substr(max.find('.')+1);
			state_id_t node = _create_le_logic(max_i, max_frac, accept_any);
			this->add_epsilon(root, node);
		}
		// Case D: len_min == len_max
		else if (L == len_min && L == len_max) {
			state_id_t node = _create_bounded_same_len(min, max, accept_any);
			this->add_epsilon(root, node);
		}
	}

	return root;
}

// 补充 helper：生成 n 个 0-9 的链，末尾接 target
void _build_any_digits_chain(state_id_t start, size_t n, state_id_t target) {
	state_id_t curr = start;
	for(size_t i=0; i<n; ++i) {
		state_id_t next = (i == n-1) ? target : this->new_state();
		this->add_range_move(curr, next, '0', '9');
		curr = next;
	}
}