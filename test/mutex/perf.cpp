#include <chrono>
#include <mutex>
#include <thread>
#include <vector>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

using namespace std;
using namespace std::chrono;

struct Elem {
	mutex  mtx;
	size_t cnt = 0;
	char   padding[64-1 - (sizeof(mutex) + sizeof(size_t) - 1) % 64];
};

int main(int argc, char* argv[]) {
	size_t thrNum = 8;
	size_t dataNum = 16;
	size_t loop = 1*1000*1000;
	printf("usage: %s threads[8] dataNum[16] loop[1,000,000]\n", argv[0]);
	if (argc >= 2) {
		thrNum = atoi(argv[1]);
	}
	if (argc >= 3) {
		dataNum = atoi(argv[2]);
	}
	if (argc >= 4) {
		loop = atoi(argv[3]);
	}
	printf("threads = %zd dataNum = %zd loop = %zd\n", thrNum, dataNum, loop);

	vector<Elem> data(dataNum);
	vector<thread> thr; thr.reserve(thrNum);
	auto func = [&,loop,dataNum](unsigned ti) {
		unsigned seed = ti;
		auto d = &*data.begin();
		unsigned checksum = 0;
		for(size_t i = 0; i < loop; ++i) {
			size_t k = rand_r(&seed) % dataNum;
			d[k].mtx.lock();
			d[k].cnt++;
			d[k].mtx.unlock();
			size_t rn = 99;
			for (size_t j = 0; j < rn; ++j) {
				checksum += rand_r(&seed) % dataNum;
			}
			checksum += k;
		}
		printf("thread %3u: checksum = %11u\n", ti, checksum);
	};
	auto t0 = steady_clock::now();
	for (size_t i = 0; i < thrNum; ++i) {
		thr.emplace_back(bind(func, i));
	}
	for (size_t i = 0; i < thrNum; ++i) {
		thr[i].join();
	}
	auto t1 = steady_clock::now();
	auto ns = nanoseconds(t1 - t0).count();
	printf("all  total time = %8.3f sec\n", ns* 1.0 / 1e9);
	printf("all  avg   time = %8.3f ns\n" , ns* 1.0 / loop);
	printf("rand avg   time = %8.3f ns\n" , ns* 1.0 / loop / 100);

	t0 = steady_clock::now();
	unsigned seed = 1;
	unsigned checksum = 0; // to disable compiler optimization
	for(size_t i = 0; i < loop; ++i) {
		for (size_t j = 0; j < 100; ++j) {
			checksum += rand_r(&seed) % dataNum;
		}
	}
	t1 = steady_clock::now();
	ns = nanoseconds(t1 - t0).count();
	printf("pure rand total time = %8.3f sec, checksum = %u\n", ns*1.0 / 1e9, checksum);
	printf("pure rand  avg  time = %8.3f ns\n" , ns*1.0 / loop / 100);

	return 0;
}

