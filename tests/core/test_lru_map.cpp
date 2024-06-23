#include <terark/lru_map.hpp>
#include <terark/valvec.hpp>
#include <terark/util/profiling.hpp>
#include <thread>

using namespace terark;

struct SolveInt {
    bool operator()(fstring key, int* mem) const noexcept {
        *mem = val;
        TERARK_VERIFY_EQ(atoi(key.c_str()), val);
        return true;
    }
    int val;
};

using base_lru_map = lru_hash_strmap<int>;
class test_lru_map : public base_lru_map {
    void lru_evict(const fstring& key, int* val) override {
        m_evict_cnt++;
    }
public:
    using base_lru_map::base_lru_map;
    void test_one(fstring key, int basehits) {
        const auto& map = this->risk_get_map();
        const auto value = atoi(key.c_str());
        const auto found = map.exists(key);
        std::pair<size_t, bool> ib = lru_add(key, SolveInt{value});
        TERARK_VERIFY_EQ(ib.second, !found);
        TERARK_VERIFY_LT(ib.first, map.end_i());
        TERARK_VERIFY_EQ(this->lru_val(ib.first), value);
        TERARK_VERIFY_EQ(map.val(ib.first).m_lru_refcnt, 1);
        size_t handle2 = this->lru_get(key);
        TERARK_VERIFY_EQ(ib.first, handle2);
        TERARK_VERIFY_EQ(map.val(ib.first).m_lru_refcnt, 2);
        this->lru_release(handle2);
        this->lru_release(ib.first);
        TERARK_VERIFY_EQ(map.val(handle2).m_lru_refcnt, 0);
        TERARK_VERIFY_EQ(map.val(handle2).m_lru_hitcnt, unsigned(2 + basehits));
    }
    int m_evict_cnt = 0;
};

struct TestLruMapLock : LruMapLock {
    using LruMapLock::lock;
    using LruMapLock::unlock;
};

int main(int argc, char** argv)
{
    printf("TestLruMapLock running...\n");
    TestLruMapLock mtx;
    auto loop = getEnvLong("TestLruMapLockLoop", 100);
    auto thr_func = [&]() {
        for (long i = 0; i < loop; ++i) {
            auto t0 = qtime::now();
            mtx.lock();
            auto t1 = qtime::now();
            std::this_thread::yield();
            auto t2 = qtime::now();
            mtx.unlock();
            auto t3 = qtime::now();
            if (t0.us(t3) > 5000)
                printf("%f  %f  %f\n", t0.uf(t1), t1.uf(t2), t2.uf(t3));
        }
    };
    const long num_thr = 100;
    valvec<std::thread> thr_vec(num_thr, valvec_reserve());
    for (long i = 0; i < num_thr; ++i) {
        thr_vec.unchecked_emplace_back(thr_func);
    }
    for (long i = 0; i < num_thr; ++i) {
        thr_vec[i].join();
    }
    printf("TestLruMapLock finished!\n");

    printf("test_lru_map running...\n");
    test_lru_map lru;
    lru.set_lru_cap(32);
    for (int i = 0; i < 32; i++) {
        lru.test_one(std::to_string(i), 0);
    }
    TERARK_VERIFY_EQ(lru.m_evict_cnt, 0);
    for (int i = 0; i < 32; i++) {
        lru.test_one(std::to_string(i), 2);
    }
    TERARK_VERIFY_EQ(lru.m_evict_cnt, 0);
    for (int i = 32; i < 64; i++) {
        lru.test_one(std::to_string(i), 0);
    }
    TERARK_VERIFY_EQ(lru.m_evict_cnt, 32);
    TERARK_VERIFY_EQ(lru.lru_get("abc"), size_t(-1));
    TERARK_VERIFY_EQ(lru.risk_get_map().size(), 32);
    printf("test_lru_map passed!\n");
	return 0;
}
