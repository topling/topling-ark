#include <terark/fsa/automata.hpp>
#include <terark/fsa/nfa.hpp>
#include <terark/fsa/onfly_minimize.hpp>
#include <terark/util/linebuf.hpp>
#include <terark/util/profiling.hpp>

using namespace std;
using namespace terark;


// 简单的辅助大数运算，用于生成测试用例
namespace BigIntUtils {
    // 比较两个数值字符串（处理符号）
    // 返回 -1 (a<b), 0 (a==b), 1 (a>b)
    int compare(string a, string b) {
        if (a == b) return 0;
        bool negA = (a[0] == '-');
        bool negB = (b[0] == '-');
        if (negA && !negB) return -1;
        if (!negA && negB) return 1;
        if (negA && negB) {
            // 都是负数，绝对值大的反而小
            if (a.length() != b.length()) return a.length() > b.length() ? -1 : 1;
            return a > b ? -1 : 1;
        }
        // 都是正数
        string pa = (a[0] == '+') ? a.substr(1) : a;
        string pb = (b[0] == '+') ? b.substr(1) : b;
        if (pa.length() != pb.length()) return pa.length() < pb.length() ? -1 : 1;
        return pa < pb ? -1 : 1;
    }

    // 正整数加1
    string add_one_abs(string s) {
        int n = s.length();
        int i = n - 1;
        while (i >= 0) {
            if (s[i] < '9') {
                s[i]++;
                return s;
            }
            s[i] = '0';
            i--;
        }
        return "1" + s;
    }

    // 正整数减1 (假设 s > 0)
    string sub_one_abs(string s) {
        if (s == "1") return "0"; // 特殊情况
        int n = s.length();
        int i = n - 1;
        while (i >= 0) {
            if (s[i] > '0') {
                s[i]--;
                // 处理前导0，例如 10 -> 09 -> 9
                if (s[0] == '0' && s.length() > 1) return s.substr(1);
                return s;
            }
            s[i] = '9';
            i--;
        }
        return s;
    }

    // 字符串数值加1
    string add_one(string s) {
        if (s == "-1") return "0";
        if (s[0] == '-') {
            // 负数加1 -> 绝对值减1
            string res = sub_one_abs(s.substr(1));
            return (res == "0") ? "0" : "-" + res;
        }
        // 正数
        string val = (s[0] == '+') ? s.substr(1) : s;
        if (val == "0") return "1"; // "0" -> "1" (0无符号)
        return add_one_abs(val);
    }

    // 字符串数值减1
    string sub_one(string s) {
        if (s == "0") return "-1";
        if (s[0] == '-') {
            // 负数减1 -> 绝对值加1
            return "-" + add_one_abs(s.substr(1));
        }
        // 正数
        string val = (s[0] == '+') ? s.substr(1) : s;
        return sub_one_abs(val);
    }
}

void generate_test_set(string min, string max, vector<string>* accept_set, vector<string>* reject_set) {
    // 1. 清理集合
    if (!accept_set || !reject_set) return;
    accept_set->clear();
    reject_set->clear();

    // 2. 规范化输入以便比较
    string norm_min = min;
    string norm_max = max;
    if (norm_min.length() > 1 && norm_min[0] == '+') norm_min = norm_min.substr(1);
    if (norm_max.length() > 1 && norm_max[0] == '+') norm_max = norm_max.substr(1);

    // 确保 min <= max
    if (BigIntUtils::compare(norm_min, norm_max) > 0) {
        // 如果 min > max，全部都是拒绝 (或者仅测试空集)
        // 这种情况下，我们假设没有 accept，只有 reject
        reject_set->push_back(norm_min);
        reject_set->push_back(norm_max);
        reject_set->push_back("0");
        return;
    }

    // 3. 生成 Accept Set (边界点)
    accept_set->push_back(norm_min);
    accept_set->push_back(norm_max);

    // 中间点测试
    string min_plus = BigIntUtils::add_one(norm_min);
    if (BigIntUtils::compare(min_plus, norm_max) < 0) accept_set->push_back(min_plus);

    string max_minus = BigIntUtils::sub_one(norm_max);
    if (BigIntUtils::compare(max_minus, norm_min) > 0) accept_set->push_back(max_minus);

    // 如果区间跨越 0，确保测试 0, -1, 1
    if (BigIntUtils::compare(norm_min, "0") <= 0 && BigIntUtils::compare(norm_max, "0") >= 0) {
        accept_set->push_back("0");
    }

    // 如果区间很大，生成一些位数变化的边界
    // 例如 range 5 to 105. 测试 9, 10, 99, 100
    vector<string> length_checks = {"9", "10", "99", "100", "-9", "-10", "-99", "-100"};
    for (const string& s : length_checks) {
        if (BigIntUtils::compare(s, norm_min) >= 0 && BigIntUtils::compare(s, norm_max) <= 0) {
            accept_set->push_back(s);
        } else {
            reject_set->push_back(s); // 如果在外面，正好也是 reject 测试
        }
    }

    // 4. 生成 Reject Set (数值边界外)
    reject_set->push_back(BigIntUtils::sub_one(norm_min));
    reject_set->push_back(BigIntUtils::add_one(norm_max));

    // 5. 生成 Reject Set (格式错误)
    // 前导零
    reject_set->push_back("00");
    reject_set->push_back("01");
    if (BigIntUtils::compare(norm_min, "0") > 0) {
        reject_set->push_back("0" + norm_min); // e.g. "0123"
    }
    // 负零
    reject_set->push_back("-0");
    reject_set->push_back("+0");

    // 带+号的数字 (根据题目要求，生成的NFA识别的字符串不含前导0，通常标准整数输出也不带+)
    // 如果题目意图是NFA只接受标准整数表示
    if (BigIntUtils::compare("0", norm_max) < 0) {
        reject_set->push_back("+1");
    }

    // 空串和非法字符
    reject_set->push_back("");
    reject_set->push_back("-");
    reject_set->push_back("+");
    reject_set->push_back("abc");
    reject_set->push_back("12a");

    // 去重
    sort(accept_set->begin(), accept_set->end());
    accept_set->erase(unique(accept_set->begin(), accept_set->end()), accept_set->end());

    sort(reject_set->begin(), reject_set->end());
    reject_set->erase(unique(reject_set->begin(), reject_set->end()), reject_set->end());

    // 确保 reject 和 accept 无交集 (以防万一 BigInt 逻辑有 bug)
    // 这里的逻辑主要是防止 min_plus 超过 max 导致的重叠，虽然上面加了 if
    // 简单的做法是移除 reject set 中出现在 accept set 的元素
    vector<string> clean_reject;
    for (const string& r : *reject_set) {
        bool found = false;
        for (const string& a : *accept_set) {
            if (r == a) { found = true; break; }
        }
        if (!found) clean_reject.push_back(r);
    }
    *reject_set = clean_reject;
}

void test(fstring min, fstring max) {
    terark::profiling pf;
    typedef Automata<State32> DFA;
    typedef GenericNFA<> NFA;
    NFA nfa1;
    DFA dfa1;
    nfa1.erase_all();
    size_t root = nfa1.create_min_max_decimal(min, max);
    TERARK_VERIFY_EQ(root, initial_state);
    {
        long t0 = pf.now();
        size_t ps_size = nfa1.make_dfa(&dfa1, SIZE_MAX, SIZE_MAX);
        long t1 = pf.now();
        printf("nfa.make_dfa time = %f'ms dfa states=%zd transition=%zd mem=%zd power_set.size=%zd\n",
                pf.mf(t0, t1), dfa1.total_states(), dfa1.total_transitions(),
                dfa1.mem_size(), ps_size);
        long t2 = pf.now();
        dfa1.graph_dfa_minimize();
        long t3 = pf.now();
        printf("dfa.minimize time = %f'ms dfa states=%zd transition=%zd mem=%zd\n",
                pf.mf(t2, t3), dfa1.total_states(), dfa1.total_transitions(),
                dfa1.mem_size());
    }
    {
        vector<string> accept_set, reject_set;
        long t0 = pf.now();
        generate_test_set(min.str(), max.str(), &accept_set, &reject_set);
        string accept_all, reject_all;
        for (auto& val : accept_set) {
            TERARK_VERIFY_S(dfa1.accept(val), "%s", val);
            accept_all += val;
            accept_all += " ";
        }
        for (auto& val : reject_set) {
            TERARK_VERIFY_S(!dfa1.accept(val), "%s", val);
            reject_all += val;
            reject_all += " ";
        }
        long t1 = pf.now();
        printf("test time = %f'ms\n", pf.mf(t0, t1));
        printf("accept = %s\n", accept_all.c_str());
        printf("reject = %s\n", reject_all.c_str());
    }
    std::string stem = "min-" + min + "-max-" + max;
    dfa1.write_dot_file(stem + ".txt");
    dfa1.path_zip("DFS");
    dfa1.write_dot_file(stem + ".dot");
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        test("123", "123");
        test("321", "1321");
        test("-123", "-123");
        test("-523", "+3213");
        test("-312", "+21");
        test("-312", "0");
        test("0", "+321");
        test("0", "0");
        test("-23423345342", "721389479182374981274");
        return 0;
    }
    test(argv[1], argv[2]);
    return 0;
}

