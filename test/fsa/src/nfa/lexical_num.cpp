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

DFA make_ge(fstring min) {
    NFA nfa1; // NFA 构造时创建了一个 initial_state(=0)，但这里我们不需要它，所以将所有 state 清空
    nfa1.erase_all(); // 必须清空所有 state
    size_t root = nfa1.create_lexical_ge(min);
    // root 是 create_min_realnum 中第一次调用 new_state 返回的状态，在此测试中等于 initial_state
     TERARK_VERIFY_EQ(root, initial_state);
    return make_dfa(nfa1);
}
DFA make_le(fstring max) {
    NFA nfa1;
    nfa1.erase_all();
    size_t root = nfa1.create_lexical_le(max);
    TERARK_VERIFY_EQ(root, initial_state);
    return make_dfa(nfa1);
}

void test_ge() {
    printf("test_ge...\n");
    DFA dfa = make_ge("");
    TERARK_VERIFY("min = empty" && dfa.accept(""));
    TERARK_VERIFY("min = empty" && dfa.accept("0"));
    TERARK_VERIFY("min = empty" && dfa.accept("00"));
    TERARK_VERIFY("min = empty" && dfa.accept("000"));
    TERARK_VERIFY("min = empty" && dfa.accept("9"));
    dfa = make_ge("0");
    TERARK_VERIFY("min = 0" && dfa.accept("0"));
    TERARK_VERIFY("min = 0" && dfa.accept("00"));
    TERARK_VERIFY("min = 0" && dfa.accept("000"));
    TERARK_VERIFY("min = 0" && dfa.accept("9"));
    TERARK_VERIFY("min = 0" &&!dfa.accept(""));
    dfa = make_ge("12345");
    TERARK_VERIFY("min = 12345" && dfa.accept("12345"));
    TERARK_VERIFY("min = 12345" && dfa.accept("123450"));
    TERARK_VERIFY("min = 12345" && dfa.accept("1234500"));
    TERARK_VERIFY("min = 12345" && dfa.accept("2"));
    TERARK_VERIFY("min = 12345" && dfa.accept("3"));
    TERARK_VERIFY("min = 12345" && dfa.accept("4"));
    TERARK_VERIFY("min = 12345" && dfa.accept("5"));
    TERARK_VERIFY("min = 12345" && dfa.accept("6"));
    TERARK_VERIFY("min = 12345" && dfa.accept("7"));
    TERARK_VERIFY("min = 12345" && dfa.accept("8"));
    TERARK_VERIFY("min = 12345" && dfa.accept("9"));
    TERARK_VERIFY("min = 12345" && dfa.accept("2234500"));
    TERARK_VERIFY("min = 12345" && dfa.accept("9234500"));
    TERARK_VERIFY("min = 12345" &&!dfa.accept("12344"));
    TERARK_VERIFY("min = 12345" &&!dfa.accept("123449999999"));
    dfa = make_ge("10000");
    TERARK_VERIFY("min = 10000" && dfa.accept("10000"));
    TERARK_VERIFY("min = 10000" && dfa.accept("100000"));
    TERARK_VERIFY("min = 10000" && dfa.accept("10001"));
    TERARK_VERIFY("min = 10000" && dfa.accept("2"));
    TERARK_VERIFY("min = 10000" && dfa.accept("9"));
    dfa = make_ge("99999");
    TERARK_VERIFY("min = 99999" && dfa.accept("99999"));
    TERARK_VERIFY("min = 99999" && dfa.accept("999990"));
    TERARK_VERIFY("min = 99999" && dfa.accept("9999900"));
    TERARK_VERIFY("min = 99999" && dfa.accept("9999999"));
    TERARK_VERIFY("min = 99999" &&!dfa.accept("8"));
    TERARK_VERIFY("min = 99999" &&!dfa.accept("9"));
    TERARK_VERIFY("min = 99999" &&!dfa.accept("9999"));
    TERARK_VERIFY("min = 99999" &&!dfa.accept("100000"));

    printf("test_ge...done\n");
}

void test_le() {
    printf("test_le...\n");
    DFA dfa = make_le("");
    TERARK_VERIFY_EQ(dfa.total_states(), 1);
    TERARK_VERIFY("max = empty" && dfa.accept(""));
    TERARK_VERIFY("max = empty" &&!dfa.accept("0"));
    TERARK_VERIFY("max = empty" &&!dfa.accept("1"));
    TERARK_VERIFY("max = empty" &&!dfa.accept("2"));
    TERARK_VERIFY("max = empty" &&!dfa.accept("3"));
    TERARK_VERIFY("max = empty" &&!dfa.accept("4"));
    dfa = make_le("0");
    TERARK_VERIFY("max = 0" && dfa.accept("0"));
    TERARK_VERIFY("max = 0" && dfa.accept(""));
    TERARK_VERIFY("max = 0" &&!dfa.accept("00"));
    TERARK_VERIFY("max = 0" &&!dfa.accept("1"));
    TERARK_VERIFY("max = 0" &&!dfa.accept("2"));
    TERARK_VERIFY("max = 0" &&!dfa.accept("3"));
    TERARK_VERIFY("max = 0" &&!dfa.accept("4"));
    TERARK_VERIFY("max = 0" &&!dfa.accept("9"));
    dfa = make_le("12345");
    TERARK_VERIFY("max = 12345" && dfa.accept("12345"));
    TERARK_VERIFY("max = 12345" && dfa.accept("12344"));
    TERARK_VERIFY("max = 12345" && dfa.accept("123449"));
    TERARK_VERIFY("max = 12345" && dfa.accept("123449999"));
    dfa = make_le("10000");
    TERARK_VERIFY("max = 10000" && dfa.accept("10000"));
    TERARK_VERIFY("max = 09"    && dfa.accept("09"));
    TERARK_VERIFY("max = 09000" && dfa.accept("09000"));
    TERARK_VERIFY("max = 09999" && dfa.accept("09999"));
    dfa = make_le("99999");
    TERARK_VERIFY("max = 99999" && dfa.accept("99999"));
    TERARK_VERIFY("max = 99999" && dfa.accept("99998"));
    TERARK_VERIFY("max = 99999" && dfa.accept("100000"));
    TERARK_VERIFY("max = 99999" &&!dfa.accept("999990"));
    printf("test_le...done\n");
}

int main() {
    test_ge();
    test_le();
    return 0;
}
