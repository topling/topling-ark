#include <terark/hash_strmap.hpp>

using namespace terark;

bool g_check_leak = true;
size_t g_cnt = 0;
struct TestValue {
    int val;
    TestValue(int v) : val(v) { g_cnt++; }
    TestValue(const TestValue& y) : val(y.val) { g_cnt++; }
    ~TestValue() { g_cnt--; }
    operator int() const { return val; }
};

template<class Value, class ValuePlace, class CopyStrategy>
void test(const char* prog) {
{
    hash_strmap<Value, fstring_func::hash_align, fstring_func::equal_align,
                ValuePlace, CopyStrategy, unsigned, size_t, true> hsm;
    #define TEST_LEAK(realcnt) do { if (g_check_leak) TERARK_VERIFY_EQ(realcnt, g_cnt); } while (0)
    #define TEST_INSERT_OK(k,v,expected_size) do { \
        TEST_LEAK(hsm.size()); \
        auto ib = hsm.insert_i(k, v);  \
        TEST_LEAK(hsm.size()+0); \
        TERARK_VERIFY_EQ(ib.second, true); \
        TERARK_VERIFY_NE(hsm.end_i(), ib.first); \
        TERARK_VERIFY_EQ(hsm.size(), expected_size); \
    } while (0)
    // freelist is disabled by default, but we make it explicit here
    hsm.disable_freelist();
    TEST_INSERT_OK("a", 1, 1);
    TEST_INSERT_OK("b", 2, 2);
    TEST_LEAK(hsm.size());
                  hsm.erase_i(0);  TERARK_VERIFY_EQ(hsm.size(), 1);
    TEST_LEAK(hsm.size());
    TERARK_VERIFY(hsm.erase("b")); TERARK_VERIFY_EQ(hsm.size(), 0);
    TEST_LEAK(hsm.size());
    auto copy_hsm_test = hsm;

    // test freelist
    hsm.enable_freelist(1);
    TERARK_VERIFY_EQ(hsm.get_fastleng(), 1);
    for(size_t i = 0; i <= 10; i++) {
        TEST_INSERT_OK(fstring("0123456789", i), i, i+1);
    }
    TEST_INSERT_OK("012345678901234567890123456789", 11, 12);
    TEST_INSERT_OK("0123456789012345678901234567890", 12, 13);
    for(size_t i = 0; i < 8; i++) {
        size_t c = hsm.erase(fstring("0123456789", i));
        TERARK_VERIFY_EQ(c, 1u);
        TERARK_VERIFY_EQ(hsm.get_fastlist(i/SP_ALIGN).llen, i+1);
    }
    for(size_t i = 8; i <= 10; i++) {
        size_t c = hsm.erase(fstring("0123456789", i));
        TERARK_VERIFY_EQ(c, 1u);
        TERARK_VERIFY_EQ(hsm.get_fastlist(i/SP_ALIGN).llen, i+1-8); // hugelist
    }
    {
        size_t c = hsm.erase("012345678901234567890123456789");
        TERARK_VERIFY_EQ(c, 1u);
        TERARK_VERIFY_EQ(hsm.get_fastlist(1).llen, 4); // hugelist
    }
    TERARK_VERIFY_EQ(hsm.get_fastlist(0).llen, 8);
    TERARK_VERIFY_EQ(hsm.get_fastlist(1).llen, 4); // hugelist
    hsm.enable_freelist(SP_ALIGN*3); // enlarge fastlist
    TERARK_VERIFY_EQ(hsm.get_fastleng(), 3);
    TERARK_VERIFY_EQ(hsm.get_fastlist(0).llen, 8); // strlen(key) [ 0,  7]
    TERARK_VERIFY_EQ(hsm.get_fastlist(1).llen, 3); // strlen(key) [ 8, 15]
    TERARK_VERIFY_EQ(hsm.get_fastlist(2).llen, 0); // strlen(key) [16, 23]
    TERARK_VERIFY_EQ(hsm.get_fastlist(3).llen, 1); // hugelist

    hsm.enable_freelist(SP_ALIGN*2); // shrink fastlist
    TERARK_VERIFY_EQ(hsm.get_fastleng(), 2);
    TERARK_VERIFY_EQ(hsm.get_fastlist(0).llen, 8); // strlen(key) [ 0,  7]
    TERARK_VERIFY_EQ(hsm.get_fastlist(1).llen, 3); // strlen(key) [ 8, 15]
    TERARK_VERIFY_EQ(hsm.get_fastlist(2).llen, 1); // hugelist

    hsm.enable_freelist(8); // shrink fastlist
    TERARK_VERIFY_EQ(hsm.get_fastleng(), 1); // shrink again
    TERARK_VERIFY_EQ(hsm.get_fastlist(0).llen, 8); // strlen(key) [ 0,  7]
    TERARK_VERIFY_EQ(hsm.get_fastlist(1).llen, 4); // hugelist

    printf("test %s OK, sizeof(hsm) = %zd, sizeof(hash_strmap<..., false>) = %zd\n",
           prog, sizeof(hsm),
           sizeof(hash_strmap<Value, fstring_func::hash_align, fstring_func::equal_align, ValuePlace, CopyStrategy, unsigned, size_t, false>));
}
TEST_LEAK(0);
}

int main(int argc, char* argv[]) {
    test<TestValue, ValueInline, SafeMove>(argv[0]);
    test<TestValue, ValueOut   , SafeMove>(argv[0]);
    g_check_leak = false;
    test<int, ValueInline, FastCopy>(argv[0]);
    test<int, ValueOut   , FastCopy>(argv[0]);
    return 0;
}
