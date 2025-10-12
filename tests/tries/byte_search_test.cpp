#define _CRT_SECURE_NO_WARNINGS
#define _SCL_SECURE_NO_WARNINGS
#include <terark/fsa/fast_search_byte.hpp>
#include <vector>
int main(int argc, char* argv[]) {
    using namespace terark;
    const std::vector<byte_t> vec = {
         0,  2,  4,  6,  8, 10, 12, 14,
        16, 18, 20, 22, 24, 26, 28, 30,
        32, 34, 36, 38, 40, 42, 44, 46,
        48, 50, 52, 54, 56, 58, 60, 62,
        64, 66, 68
    };
    for (byte_t key : vec) {
        size_t pos = binary_search_byte(vec.data(), vec.size(), key);
        TERARK_VERIFY_LT(pos, vec.size());
        TERARK_VERIFY_EQ(vec[pos], key);
    }
    for (byte_t key : vec) {
        size_t pos = binary_search_byte(vec.data(), vec.size(), key + 1);
        TERARK_VERIFY_EQ(pos, vec.size());
    }
    for (byte_t key : vec) {
        size_t p1 = binary_search_byte(vec.data(), vec.size(), key);
        size_t p2 = fast_search_byte_max_35(vec.data(), vec.size(), key);
        TERARK_VERIFY_EQ(p1, p2);
    }
    for (byte_t key : vec) {
        size_t p1 = binary_search_byte(vec.data(), vec.size(), key + 1);
        size_t p2 = fast_search_byte_max_35(vec.data(), vec.size(), key + 1);
        TERARK_VERIFY_EQ(p1, vec.size());
        TERARK_VERIFY_GE(p2, vec.size());
    }
    for (size_t key = vec.back() + 2; key <= 255; key++) {
        size_t p1 = binary_search_byte(vec.data(), vec.size(), key);
        size_t p2 = fast_search_byte_max_35(vec.data(), vec.size(), key);
        TERARK_VERIFY_EQ(p1, vec.size());
        TERARK_VERIFY_GE(p2, vec.size());
    }
#if defined(__AVX512VL__) && defined(__AVX512BW__)
  #if defined(__AVX512_PREFER_256_VECTORS__)
    printf("fast_search_byte_max_35 is avx512_search_byte_max64_256 done!\n");
  #else
    printf("fast_search_byte_max_35 is avx512_search_byte_max64_512 done!\n");
  #endif
#else
    printf("no avx512, fast_search_byte_max_35 is binary_search_byte!\n");
#endif
    return 0;
}
