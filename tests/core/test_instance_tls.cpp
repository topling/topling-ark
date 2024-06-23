#include <terark/thread/instance_tls.hpp>
#include <thread>
#include <memory>

using namespace terark;

struct TlsObj {
    int val = 0;
    ~TlsObj() {
        val = 0xBDEF;
    }
};

struct BigObj {
    std::shared_ptr<instance_tls<TlsObj> > tls;
    std::mutex mtx;
};

int main() {
    valvec<std::thread> thr(16, valvec_reserve());
    valvec<BigObj> nums(2000);
    auto thread_func = [&]() {
        size_t loop = 100;
        for (size_t i = 0; i < loop; ++i)
        for (size_t j = 0; j < nums.size(); ++j) {
            auto& bo = nums[j];
            std::lock_guard<std::mutex> lock(bo.mtx);
            if (!bo.tls)
                bo.tls.reset(new instance_tls<TlsObj>());
            bo.tls->get().val++;
            if (bo.tls->get().val % 10 == 0)
                bo.tls.reset();
        }
    };
    for (size_t i = 0; i < thr.capacity(); ++i) {
        thr.unchecked_emplace_back(thread_func);
    }
    thread_func();
    for (auto& t : thr) {
        t.join();
    }
    return 0;
}
