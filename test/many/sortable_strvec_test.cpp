#include <terark/util/sortable_strvec.hpp>
#include <random>

void test_FixedLenStrVec() {
    using namespace terark;
    FixedLenStrVec fstrvec;
    fstrvec.m_strpool.resize(4*8*12*16*100);
    std::mt19937_64 rnd;
    for (size_t fixlen : {4, 8, 12, 16, 24, 32, 40, 48}) {
      for (size_t valuelen : {2, 4, 6, 8, 12, 16, 24, 32}) {
        if (valuelen >= fixlen) continue;
        fstrvec.m_fixlen = fixlen;
        fstrvec.m_size = fstrvec.m_strpool.size() / fixlen;
        printf("test_FixedLenStrVec: testing fixlen = %2zd, keylen = %2zd, valuelen = %2zd, num = %6zd...\n"
             , fixlen, fixlen - valuelen, valuelen, fstrvec.m_size);
        for (size_t i = 0; i < fstrvec.m_strpool.size() / 8; ++i) {
            ((uint64_t*)fstrvec.m_strpool.data())[i] = rnd();
        }
        fstrvec.sort(valuelen);
        for (size_t i = 1; i < fstrvec.m_size; ++i) {
            fstring prev = fstrvec[i-1];
            fstring curr = fstrvec[i-0];
            TERARK_VERIFY_F(memcmp(prev.p, curr.p, fixlen - valuelen) <= 0,
                            "i = %zd, fixlen = %zd", i, fixlen);
        }
        printf("test_FixedLenStrVec: testing fixlen = %2zd, keylen = %2zd, valuelen = %2zd, num = %6zd... passed\n"
             , fixlen, fixlen - valuelen, valuelen, fstrvec.m_size);
      }
    }

    for (size_t fixlen = 1; fixlen < 20; fixlen++) {
        fstrvec.m_fixlen = fixlen;
        fstrvec.m_size = fstrvec.m_strpool.capacity() / fixlen;
        fstrvec.m_strpool.resize(fixlen * fstrvec.m_size);
        printf("test_FixedLenStrVec: testing lower_bound & upper_bound fixlen = %2zd, num = %6zd...\n"
             , fixlen, fstrvec.m_size);
        for (size_t i = 0; i < fstrvec.m_strpool.size() / 8; ++i) {
            ((uint64_t*)fstrvec.m_strpool.data())[i] = rnd();
        }
        fstrvec.sort();
        for (size_t i = 0; i < fstrvec.m_size; ++i) {
            size_t lo = fstrvec.lower_bound(fstrvec[i]);
            TERARK_VERIFY_LE(lo, i); // dup key makes lo < i
            TERARK_VERIFY_S_EQ(fstrvec[lo], fstrvec[i]);
            TERARK_VERIFY_GT(fstrvec.upper_bound(fstrvec[i]), i);
            std::string key = fstrvec[i].str();
            key.push_back('\0');
            TERARK_VERIFY_GT(fstrvec.lower_bound(key), i);
            TERARK_VERIFY_GE(fstrvec.upper_bound(key), fstrvec.lower_bound(key));
            key.pop_back();
            key.pop_back();
            TERARK_VERIFY_LE(fstrvec.lower_bound(key), fstrvec.upper_bound(key));
            TERARK_VERIFY_LE(fstrvec.upper_bound(key), i);
        }
        TERARK_VERIFY_EQ(fstrvec.lower_bound(std::string()), 0);
        TERARK_VERIFY_EQ(fstrvec.lower_bound(std::string(fixlen, '\0')), 0);
        TERARK_VERIFY_EQ(fstrvec.upper_bound(std::string(fixlen, '\xFF')), fstrvec.m_size);
        printf("test_FixedLenStrVec: testing lower_bound & upper_bound fixlen = %2zd, num = %6zd... passed\n"
             , fixlen, fstrvec.m_size);
    }
}

int main() {
    test_FixedLenStrVec();
    return 0;
}