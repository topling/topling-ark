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

