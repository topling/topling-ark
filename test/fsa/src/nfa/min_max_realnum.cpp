#include <queue>
#include <terark/sso.hpp>
#include <terark/fsa/automata.hpp>
#include <terark/fsa/nfa.hpp>
#include <terark/fsa/onfly_minimize.hpp>
#include <terark/util/linebuf.hpp>
#include <terark/util/profiling.hpp>

using namespace std;
using namespace terark;
static profiling pf;

typedef Automata<State32> DFA;
typedef GenericNFA<> NFA;

static const bool g_timing = getEnvBool("TIMING");
static long g_cnt = 0;
DFA make_dfa(const NFA& nfa) {
    DFA dfa;
    long t0 = pf.now();
    size_t ps_size = nfa.make_dfa(&dfa, SIZE_MAX, SIZE_MAX);
    long t1 = pf.now();
    if (g_timing)
        printf("min-info nfa.make_dfa time = %f'ms dfa states=%zd transition=%zd mem=%zd power_set.size=%zd\n",
            pf.mf(t0, t1), dfa.total_states(), dfa.total_transitions(),
            dfa.mem_size(), ps_size);
    long t2 = pf.now();
    dfa.graph_dfa_minimize();
    nfa.write_dot_file(as_string_appender(std::string("nfa-"))|g_cnt|".dot");
    dfa.write_dot_file(as_string_appender(std::string("dfa-"))|g_cnt++|".dot");
    long t3 = pf.now();
    if (g_timing)
        printf("min-info dfa.minimize time = %f'ms dfa states=%zd transition=%zd mem=%zd\n",
            pf.mf(t2, t3), dfa.total_states(), dfa.total_transitions(),
            dfa.mem_size());
    return dfa;
}

DFA make_min(fstring min) {
    NFA nfa1; // NFA 构造时创建了一个 initial_state(=0)，但这里我们不需要它，所以将所有 state 清空
    nfa1.erase_all(); // 必须清空所有 state
    size_t root = nfa1.create_min_realnum(min);
    // root 是 create_min_realnum 中第一次调用 new_state 返回的状态，在此测试中等于 initial_state
     TERARK_VERIFY_EQ(root, initial_state);
    return make_dfa(nfa1);
}
DFA make_min_max(fstring min, fstring max) {
    NFA nfa1;
    nfa1.erase_all();
    size_t root = nfa1.create_min_max_realnum<DFA>(min, max);
    TERARK_VERIFY_EQ(root, initial_state);
    return make_dfa(nfa1);
}
DFA make_min_max_int(fstring min, fstring max) {
    NFA nfa1;
    nfa1.erase_all();
    size_t root = nfa1.create_min_max_decimal(min, max);
    TERARK_VERIFY_EQ(root, initial_state);
    return make_dfa(nfa1);
}

void test_min_max_int() {
    printf("test_min_max_int...\n");
    DFA dfa = make_min_max_int("123", "4999");
    TERARK_VERIFY("minmax is [123,4999]" &&  dfa.accept("500"));
    printf("test_min_max_int...done\n");

    dfa = make_min_max_int("9", "100");
    TERARK_VERIFY("minmax = [9,100]" &&  dfa.accept("9"));
    TERARK_VERIFY("minmax = [9,100]" &&  dfa.accept("10"));
    TERARK_VERIFY("minmax = [9,100]" &&  dfa.accept("99"));
    TERARK_VERIFY("minmax = [9,100]" &&  dfa.accept("100"));
    TERARK_VERIFY("minmax = [9,100]" && !dfa.accept("8.9"));
    TERARK_VERIFY("minmax = [9,100]" && !dfa.accept("101"));
}

void test_min() {
    printf("test_min...\n");
    DFA dfa = make_min("4999.99");
    // 测试用例如下
    TERARK_VERIFY("min is 4999.99" &&  dfa.accept("4999.99"));
    TERARK_VERIFY("min is 4999.99" &&  dfa.accept("5000"));
    TERARK_VERIFY("min is 4999.99" && !dfa.accept("500"));
    // 测试整数 min
    dfa = make_min("100");
    TERARK_VERIFY("min = 100" &&  dfa.accept("100"));
    TERARK_VERIFY("min = 100" &&  dfa.accept("101"));
    TERARK_VERIFY("min = 100" && !dfa.accept("99"));
    TERARK_VERIFY("min = 100" && !dfa.accept("99.9"));
    dfa = make_min("123.456");
    TERARK_VERIFY("min = 123.456" &&  dfa.accept("500"));
    // 测试小数 min
    dfa = make_min("0.5");
    TERARK_VERIFY("min = 0.5" &&  dfa.accept("0.5"));
    TERARK_VERIFY("min = 0.5" &&  dfa.accept("0.6"));
    TERARK_VERIFY("min = 0.5" &&  dfa.accept("1"));
    TERARK_VERIFY("min = 0.5" && !dfa.accept("0.4"));
    TERARK_VERIFY("min = 0.5" && !dfa.accept("0.49"));
    // 更多测试
    dfa = make_min("0");
    TERARK_VERIFY("min = 0" &&  dfa.accept("0"));
    TERARK_VERIFY("min = 0" &&  dfa.accept("0.0"));
    TERARK_VERIFY("min = 0" &&  dfa.accept("1"));
    TERARK_VERIFY("min = 0" && !dfa.accept("-1"));
    dfa = make_min("999.999");
    TERARK_VERIFY("min = 999.999" &&  dfa.accept("999.999"));
    TERARK_VERIFY("min = 999.999" &&  dfa.accept("1000"));
    TERARK_VERIFY("min = 999.999" && !dfa.accept("999.998"));
    // 更多测试用例
    dfa = make_min("0.001");
    TERARK_VERIFY("min = 0.001" &&  dfa.accept("0.001"));
    TERARK_VERIFY("min = 0.001" &&  dfa.accept("0.002"));
    TERARK_VERIFY("min = 0.001" &&  dfa.accept("1"));
    TERARK_VERIFY("min = 0.001" && !dfa.accept("0.0009"));
    TERARK_VERIFY("min = 0.001" && !dfa.accept("0.00099"));

    // 测试边界情况：非常接近的数值
    dfa = make_min("0.999999");
    TERARK_VERIFY("min = 0.999999" &&  dfa.accept("0.999999"));
    TERARK_VERIFY("min = 0.999999" &&  dfa.accept("1"));
    TERARK_VERIFY("min = 0.999999" && !dfa.accept("0.9999989"));

    // 请添加更多测试，用不同的 min 参数，不同的测试字符串等

    printf("test_min... done\n");
}

void test_min_max() {
    printf("test_min_max...\n");
    DFA dfa = make_min_max("123.456", "4999.99");
    TERARK_VERIFY("minmax is [123.456, 4999.99]" && !dfa.accept("123"));
    TERARK_VERIFY("minmax is [123.456, 4999.99]" &&  dfa.accept("123.456"));
    TERARK_VERIFY("minmax is [123.456, 4999.99]" &&  dfa.accept("500.456"));
    TERARK_VERIFY("minmax is [123.456, 4999.99]" &&  dfa.accept("500"));
    TERARK_VERIFY("minmax is [123.456, 4999.99]" &&  dfa.accept("4999.99"));
    TERARK_VERIFY("minmax is [123.456, 4999.99]" && !dfa.accept("4999.991"));
    TERARK_VERIFY("minmax is [123.456, 4999.99]" && !dfa.accept("49999"));
    TERARK_VERIFY("minmax is [123.456, 4999.99]" && !dfa.accept("5000"));
    // 测试 min = max
    dfa = make_min_max("100", "100");
    TERARK_VERIFY("minmax = [100,100]" &&  dfa.accept("100"));
    TERARK_VERIFY("minmax = [100,100]" && !dfa.accept("99.9"));
    TERARK_VERIFY("minmax = [100,100]" && !dfa.accept("100.1"));
    //TERARK_VERIFY("minmax = [100,100]" &&  dfa.accept("100.0")); // 尾随零
    // 测试整数区间
    dfa = make_min_max("50", "100");
    TERARK_VERIFY("minmax = [50,100]" &&  dfa.accept("50"));
    TERARK_VERIFY("minmax = [50,100]" &&  dfa.accept("75"));
    TERARK_VERIFY("minmax = [50,100]" &&  dfa.accept("100"));
    TERARK_VERIFY("minmax = [50,100]" && !dfa.accept("49.9"));
    TERARK_VERIFY("minmax = [50,100]" && !dfa.accept("100.1"));
    // 测试小数区间
    dfa = make_min_max("0.1", "0.2");
    TERARK_VERIFY("minmax = [0.1,0.2]" &&  dfa.accept("0.1"));
    TERARK_VERIFY("minmax = [0.1,0.2]" &&  dfa.accept("0.15"));
    TERARK_VERIFY("minmax = [0.1,0.2]" &&  dfa.accept("0.2"));
    TERARK_VERIFY("minmax = [0.1,0.2]" && !dfa.accept("0.09"));
    TERARK_VERIFY("minmax = [0.1,0.2]" && !dfa.accept("0.21"));

    dfa = make_min_max("0.1234", "0.2345");
    TERARK_VERIFY("minmax = [0.1234,0.2345]" &&  dfa.accept("0.1234"));
    TERARK_VERIFY("minmax = [0.1234,0.2345]" &&  dfa.accept("0.13"));
    TERARK_VERIFY("minmax = [0.1234,0.2345]" &&  dfa.accept("0.234499"));
    TERARK_VERIFY("minmax = [0.1234,0.2345]" &&  dfa.accept("0.2345"));
    TERARK_VERIFY("minmax = [0.1234,0.2345]" && !dfa.accept("0.123399"));
    TERARK_VERIFY("minmax = [0.1234,0.2345]" && !dfa.accept("0.234500001"));

    // 更多测试
    // 测试 min 为整数，max 为小数
    dfa = make_min_max("100", "100.5");
    TERARK_VERIFY("minmax = [100,100.5]" &&  dfa.accept("100"));
    TERARK_VERIFY("minmax = [100,100.5]" &&  dfa.accept("100.0"));
    TERARK_VERIFY("minmax = [100,100.5]" &&  dfa.accept("100.5"));
    TERARK_VERIFY("minmax = [100,100.5]" && !dfa.accept("99.9"));
    TERARK_VERIFY("minmax = [100,100.5]" && !dfa.accept("100.6"));
    // 测试 min 为小数，max 为整数
    dfa = make_min_max("0.5", "1");
    TERARK_VERIFY("minmax = [0.5,1]" &&  dfa.accept("0.5"));
    TERARK_VERIFY("minmax = [0.5,1]" &&  dfa.accept("0.75"));
    TERARK_VERIFY("minmax = [0.5,1]" &&  dfa.accept("1"));
    TERARK_VERIFY("minmax = [0.5,1]" && !dfa.accept("0.49"));
    TERARK_VERIFY("minmax = [0.5,1]" && !dfa.accept("1.1"));
    // 测试大范围
    dfa = make_min_max("0", "99999");
    TERARK_VERIFY("minmax = [0,99999]" &&  dfa.accept("0"));
    TERARK_VERIFY("minmax = [0,99999]" &&  dfa.accept("12345"));
    TERARK_VERIFY("minmax = [0,99999]" &&  dfa.accept("99999"));
    TERARK_VERIFY("minmax = [0,99999]" && !dfa.accept("100000"));
    // 测试小数位长度不同的情况
    dfa = make_min_max("0.1", "0.123");
    TERARK_VERIFY("minmax = [0.1,0.123]" &&  dfa.accept("0.1"));
    TERARK_VERIFY("minmax = [0.1,0.123]" &&  dfa.accept("0.12"));
    TERARK_VERIFY("minmax = [0.1,0.123]" &&  dfa.accept("0.123"));
    TERARK_VERIFY("minmax = [0.1,0.123]" && !dfa.accept("0.09"));
    TERARK_VERIFY("minmax = [0.1,0.123]" && !dfa.accept("0.124"));
    // 注意：0.1230 应该被接受（尾随零）
    //TERARK_VERIFY("minmax = [0.1,0.123]" &&  dfa.accept("0.123"));

    // 测试整数部分长度不同的情况
    dfa = make_min_max("9", "100");
    TERARK_VERIFY("minmax = [9,100]" &&  dfa.accept("9"));
    TERARK_VERIFY("minmax = [9,100]" &&  dfa.accept("9.99"));
    TERARK_VERIFY("minmax = [9,100]" &&  dfa.accept("10"));
    TERARK_VERIFY("minmax = [9,100]" &&  dfa.accept("10.00001"));
    TERARK_VERIFY("minmax = [9,100]" &&  dfa.accept("99"));
    TERARK_VERIFY("minmax = [9,100]" &&  dfa.accept("99.99999"));
    TERARK_VERIFY("minmax = [9,100]" &&  dfa.accept("100"));
    TERARK_VERIFY("minmax = [9,100]" && !dfa.accept("8.9"));
    TERARK_VERIFY("minmax = [9,100]" && !dfa.accept("101"));

    // 测试极端小数区间
    dfa = make_min_max("0.000001", "0.000002");
    TERARK_VERIFY("minmax = [0.000001,0.000002]" &&  dfa.accept("0.000001"));
    TERARK_VERIFY("minmax = [0.000001,0.000002]" &&  dfa.accept("0.0000015"));
    TERARK_VERIFY("minmax = [0.000001,0.000002]" &&  dfa.accept("0.000002"));
    TERARK_VERIFY("minmax = [0.000001,0.000002]" && !dfa.accept("0.0000009"));
    TERARK_VERIFY("minmax = [0.000001,0.000002]" && !dfa.accept("0.0000021"));

    // ---------- 尾随零语义专项测试 ----------
    // 根据设计，尾部0具有语义（表示精度epsilon），因此 "1.5" 与 "1.50" 是不同的值
    dfa = make_min_max("1.5", "1.50");
    TERARK_VERIFY("tail zero sensitive: [1.5,1.50]" &&  dfa.accept("1.5"));
    TERARK_VERIFY("tail zero sensitive: [1.5,1.50]" &&  dfa.accept("1.50"));
    TERARK_VERIFY("tail zero sensitive: [1.5,1.50]" && !dfa.accept("1.49"));
    TERARK_VERIFY("tail zero sensitive: [1.5,1.50]" && !dfa.accept("1.51"));
    TERARK_VERIFY("tail zero sensitive: [1.5,1.50]" && !dfa.accept("1.500")); // 比1.50大

    dfa = make_min_max("1.5", "1.5");
    TERARK_VERIFY("[1.5,1.5]" &&  dfa.accept("1.5"));
    TERARK_VERIFY("[1.5,1.5]" && !dfa.accept("1.50")); // 大于1.5
    TERARK_VERIFY("[1.5,1.5]" && !dfa.accept("1.49"));

    dfa = make_min_max("1.50", "1.50");
    TERARK_VERIFY("[1.50,1.50]" &&  dfa.accept("1.50"));
    TERARK_VERIFY("[1.50,1.50]" && !dfa.accept("1.5")); // 小于1.50
    TERARK_VERIFY("[1.50,1.50]" && !dfa.accept("1.500"));

    // 尾随零与空小数部分的语义："2" 表示精度到个位，等价于 "2."？不，根据设计 "2" 整数部分固定，小数部分为空串（最小值）
    dfa = make_min_max("2", "2.0");
    TERARK_VERIFY("[2,2.0]" &&  dfa.accept("2"));
    TERARK_VERIFY("[2,2.0]" &&  dfa.accept("2.0"));
    TERARK_VERIFY("[2,2.0]" && !dfa.accept("1.9"));
    TERARK_VERIFY("[2,2.0]" && !dfa.accept("2.00")); // 大于2.0
    TERARK_VERIFY("[2,2.0]" && !dfa.accept("2.1"));

    // ---------- 零的各种表示 ----------
    dfa = make_min_max("0", "0");
    TERARK_VERIFY("[0,0]" &&  dfa.accept("0"));
    TERARK_VERIFY("[0,0]" && !dfa.accept("0.0")); // 大于0
    TERARK_VERIFY("[0,0]" && !dfa.accept("00"));  // 前导零非法

    dfa = make_min_max("0.0", "0.0");
    TERARK_VERIFY("[0.0,0.0]" &&  dfa.accept("0.0"));
    TERARK_VERIFY("[0.0,0.0]" && !dfa.accept("0"));
    TERARK_VERIFY("[0.0,0.0]" && !dfa.accept("0.00"));

    dfa = make_min_max("0", "0.0");
    TERARK_VERIFY("[0,0.0]" &&  dfa.accept("0"));
    TERARK_VERIFY("[0,0.0]" &&  dfa.accept("0.0"));
    TERARK_VERIFY("[0,0.0]" && !dfa.accept("0.00"));
    TERARK_VERIFY("[0,0.0]" && !dfa.accept("00"));

    // ---------- 长数字与大区间 ----------
    dfa = make_min_max("1234567890", "1234567899");
    TERARK_VERIFY("long integer range" &&  dfa.accept("1234567890"));
    TERARK_VERIFY("long integer range" &&  dfa.accept("1234567895"));
    TERARK_VERIFY("long integer range" &&  dfa.accept("1234567899"));
    TERARK_VERIFY("long integer range" && !dfa.accept("1234567889"));
    TERARK_VERIFY("long integer range" && !dfa.accept("1234567900"));

    // 大跨度区间，整数部分长度从1到10
    dfa = make_min_max("5", "10000000000");
    TERARK_VERIFY("huge span" &&  dfa.accept("5"));
    TERARK_VERIFY("huge span" &&  dfa.accept("999"));
    TERARK_VERIFY("huge span" &&  dfa.accept("9999999999"));
    TERARK_VERIFY("huge span" &&  dfa.accept("10000000000"));
    TERARK_VERIFY("huge span" && !dfa.accept("4.999"));
    TERARK_VERIFY("huge span" && !dfa.accept("10000000001"));

    // ---------- 特殊边界：仅小数点差异 ----------
    dfa = make_min_max("0.123", "0.124");
    TERARK_VERIFY("[0.123,0.124]" &&  dfa.accept("0.123"));
    TERARK_VERIFY("[0.123,0.124]" &&  dfa.accept("0.1235"));
    TERARK_VERIFY("[0.123,0.124]" &&  dfa.accept("0.124"));
    TERARK_VERIFY("[0.123,0.124]" && !dfa.accept("0.1229"));
    TERARK_VERIFY("[0.123,0.124]" && !dfa.accept("0.1241"));

    // ---------- 整数长度差1，且min整数部分为9...9的情况 ----------
    dfa = make_min_max("99", "100");
    TERARK_VERIFY("[99,100]" &&  dfa.accept("99"));
    TERARK_VERIFY("[99,100]" &&  dfa.accept("99.999"));
    TERARK_VERIFY("[99,100]" &&  dfa.accept("100"));
    TERARK_VERIFY("[99,100]" && !dfa.accept("98.999"));
    TERARK_VERIFY("[99,100]" && !dfa.accept("100.1"));

    dfa = make_min_max("999", "1000");
    TERARK_VERIFY("[999,1000]" &&  dfa.accept("999"));
    TERARK_VERIFY("[999,1000]" &&  dfa.accept("999.9999"));
    TERARK_VERIFY("[999,1000]" &&  dfa.accept("1000"));
    TERARK_VERIFY("[999,1000]" && !dfa.accept("998.99"));
    TERARK_VERIFY("[999,1000]" && !dfa.accept("1001"));

    // ---------- min 小数部分为空，max 小数部分非空 ----------
    dfa = make_min_max("10", "10.001");
    TERARK_VERIFY("[10,10.001]" &&  dfa.accept("10"));
    TERARK_VERIFY("[10,10.001]" &&  dfa.accept("10.0"));
    TERARK_VERIFY("[10,10.001]" &&  dfa.accept("10.000"));
    TERARK_VERIFY("[10,10.001]" &&  dfa.accept("10.001"));
    TERARK_VERIFY("[10,10.001]" && !dfa.accept("9.999"));
    TERARK_VERIFY("[10,10.001]" && !dfa.accept("10.002"));

    // ---------- min 和 max 均为整数，但 min 的小数部分隐含最小值 ----------
    dfa = make_min_max("100", "200");
    TERARK_VERIFY("[100,200] accept 100.0? (semantics: 100.0 > 100)" && dfa.accept("100.0"));
    TERARK_VERIFY("[100,200] accept 100?" && dfa.accept("100"));
    TERARK_VERIFY("[100,200] accept 200?" && dfa.accept("200"));
    TERARK_VERIFY("[100,200] accept 200.0?" && !dfa.accept("200.0"));

    // ---------- 非法格式（确保不崩溃，返回false） ----------
    // 前导零
    TERARK_VERIFY("leading zero" && !dfa.accept("0100"));
    TERARK_VERIFY("leading zero" && !dfa.accept("00"));
    TERARK_VERIFY("leading zero" && !dfa.accept("01.5"));
    // 空串
    TERARK_VERIFY("empty string" && !dfa.accept(""));
    // 只有小数点
    TERARK_VERIFY("only dot" && !dfa.accept("."));
    TERARK_VERIFY("only dot" && !dfa.accept(".5"));  // 根据文法可能不允许省略整数部分
    // 多个小数点
    TERARK_VERIFY("multiple dots" && !dfa.accept("1.2.3"));
    // 非数字字符
    TERARK_VERIFY("non-digit" && !dfa.accept("12a"));
    TERARK_VERIFY("non-digit" && !dfa.accept("12.3a"));

    // 请添加更多测试，用不同的 min,max 参数，不同的测试字符串等

    printf("test_min_max... done\n");
}

int main() {
    test_min_max_int();
    test_min();
    test_min_max();
    return 0;
}
