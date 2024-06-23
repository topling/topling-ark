#define _CRT_SECURE_NO_WARNINGS
#define _SCL_SECURE_NO_WARNINGS

#include <cassert>
#include <cstring>
#include <cstdio>
#include <cstdint>
#include <random>
#include <chrono>

#include <terark/bitfield_array.hpp>
#include <terark/int_vector.hpp>

using namespace terark;



template<class T, bool UNROLL = false>
struct test_rank_select {
    test_rank_select(std::string name, size_t count, size_t size) {
        rt = std::chrono::seconds(0);
        wt = std::chrono::seconds(0);
        std::mt19937_64 mt(0);
        size_t sum = 0;
        test(count, size, mt, sum);
        fprintf(stderr,
            "%s\t"
            "%7.3f\t"   //read
            "%7.3f\t"   //write
            "%12zd\t"   //count
            "%12zd\t"   //size
            "%016zX\n"  //check_sum
            , name.c_str(),
            rt.count() / count,
            wt.count() / count,
            count,
            size,
            sum
        );
    }
    static auto do_test(std::function<size_t()> f) {
        auto begin = std::chrono::high_resolution_clock::now();
        size_t sum = f();
        TERARK_UNUSED_VAR(sum);
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::duration<double, std::nano>>(end - begin);
    }
    void test(size_t count, size_t size, std::mt19937_64& mt, size_t &sum) {
        std::vector<size_t> r;
        r.resize(count);
        T t;
        t.init(mt, size);
        std::generate(r.begin(), r.end(), std::bind(std::uniform_int_distribution<size_t>(0, size - 1), std::ref(mt)));
        rt += do_test([&, count, size] {
            size_t s = 0;
            for (size_t c = 0; c < count; ) {
                size_t limit = std::min(count - c, size);
                size_t i = 0;
                if (UNROLL) {
                    for (; i + 3 < limit; i += 4)
                        s += t.get(r[i]) + t.get(r[i + 1]) + t.get(r[i + 2]) + t.get(r[i + 3]);
                }
                for (; i < limit; ++i)
                    s += t.get(r[i]);
                c += limit;
            }
            return sum += s;
        });
        wt += do_test([&, count, size] {
            for (size_t c = 0; c < count; ) {
                size_t limit = std::min(count - c, size);
                size_t i = 0;
                if (UNROLL) {
                    for (; i + 3 < limit; i += 4)
                        t.set(r[i], i & 3), t.set(r[i + 1], (i + 1) & 3), t.set(r[i + 2], (i + 2) & 3), t.set(r[i + 3], (i + 3) & 3);
                }
                for (; i < limit; ++i)
                    t.set(r[i], i & 3);
                c += limit;
            }
            return sum;
        });
    }
    std::chrono::duration<double, std::nano> rt;
    std::chrono::duration<double, std::nano> wt;
};

struct uint_vector_entity {
    UintVector uiv;

    void init(size_t size) {
        uiv = UintVector(size, 0, 3);
    }
    void init(std::mt19937_64& mt, size_t size) {
        uiv = UintVector(size, 0, 3);
        for (size_t i = 0; i < size; ++i) {
            uiv.set(i, mt() % 4);
        }
    }
    size_t get(size_t i) {
        return uiv.get(i);
    }
    void set(size_t i, size_t v) {
        uiv.set(i, v);
    }
};

struct bitfield_array_entity {
    bitfield_array<2> bfa;

    void init(size_t size) {
        bfa.resize_no_init(size);
    }
    void init(std::mt19937_64& mt, size_t size) {
        bfa.resize_no_init(size);
        for (size_t i = 0; i < size; ++i) {
            bfa.set0(i, mt() % 4);
        }
    }
    size_t get(size_t i) {
        return bfa.get0(i);
    }
    void set(size_t i, size_t v) {
        bfa.set0(i, v);
    }
};

int main(int argc, char* argv[]) {
    fprintf(stderr,
        "name\t"
        "read\t"
        "write\t"
        "count\t"
        "size\t"
        "check_sum\n");
    fprintf(stderr, "\n");

    size_t count = 400000000, maxMB = 512;
    if (argc > 1)
        count = (size_t)strtoull(argv[1], NULL, 10);
    if (argc > 2)
        maxMB = (size_t)strtoull(argv[2], NULL, 10);

if (maxMB > 4) {
    test_rank_select<uint_vector_entity   >("uint_vector",    count, 1ULL * 1024 * 1024 * maxMB);
    test_rank_select<bitfield_array_entity>("bitfield_array", count, 1ULL * 1024 * 1024 * maxMB);
}
    fprintf(stderr, "\n");
    test_rank_select<uint_vector_entity   >("uint_vector"   , count, 1ULL * 1024 * 1024 * 4);
    test_rank_select<bitfield_array_entity>("bitfield_array", count, 1ULL * 1024 * 1024 * 4);
    fprintf(stderr, "\n");
    test_rank_select<uint_vector_entity   >("uint_vector"   , count, 1ULL * 1024 * 128);
    test_rank_select<bitfield_array_entity>("bitfield_array", count, 1ULL * 1024 * 128);
    fprintf(stderr, "\n");
    test_rank_select<uint_vector_entity   >("uint_vector"   , count, 1ULL * 1024 * 4);
    test_rank_select<bitfield_array_entity>("bitfield_array", count, 1ULL * 1024 * 4);

    getchar();
}