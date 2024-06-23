#include <terark/util/profiling.hpp>
#include <thread>

int main() {
    terark::qtime t0 = terark::qtime::now();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    terark::qtime t1 = terark::qtime::now();
    printf("qtime: sleep(50ms) = %f ms\n", (t1-t0).mf());
    return 0;
}
