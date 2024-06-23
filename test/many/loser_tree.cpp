#include <terark/set_op.hpp>
#include <terark/util/profiling.cpp>

int main(int argc, char* argv[]) {
	terark::profiling pf;
	if (argc < 5) {
		fprintf(stderr, "usage: %s num_runs len_run max_rand brute_use_cache\n", argv[0]);
		return 0;
	}
	size_t num_runs = atoi(argv[1]);
	size_t len_run  = atoi(argv[2]);
	size_t max_rand = atoi(argv[3]);
	bool  brute_use_cache = atoi(argv[4]);
	std::vector<std::vector<int> > runs(num_runs);
	std::vector<int*> iruns(num_runs);
	for (size_t i = 0; i < num_runs; ++i) {
		runs[i].resize(len_run);
		for (size_t j = 0; j < len_run; ++j) {
			runs[i][j] = (rand() % max_rand) + 1;
		}
		runs[i][len_run-1] = INT_MAX;
		std::sort(runs[i].begin(), runs[i].end());
	}
	terark::multi_way::LoserTree<const int*> lt(INT_MAX);
	lt.m_ways.resize(num_runs);
	const size_t n_pass = 3;
	long t0 = pf.now();
	for (size_t pass = 0; pass < n_pass; ++pass) {
		for (size_t i = 0; i < num_runs; ++i) {
			lt.m_ways[i] = &runs[i][0];
		}
		lt.start();
		size_t n = 0;
		while (!lt.empty()) {
		#if 0
			for (size_t i = 0; i < lt.get_tree().size(); ++i) {
				int way = lt.get_tree()[i];
				printf("(%03d:%03d) ", way, *lt.m_ways[way]);
			}
			printf("\n");
		#endif
		#if 0
			printf("%d ", *lt);
		#endif
			++lt;
			++n;
		}
	//	printf("loser_tree: n=%zd\n", n);
	}
	long t1 = pf.now();
	if (brute_use_cache) {
		int cache[num_runs];
		for (size_t pass = 0; pass < n_pass; ++pass) {
			for (size_t i = 0; i < num_runs; ++i) {
				iruns[i] = &runs[i][0];
				cache[i] =  runs[i][0];
			}
			size_t n = 0;
			for (;;) {
				size_t imin = 0;
				int vmin = cache[0];
				for (size_t i = 1; i < num_runs; ++i) {
					if (vmin > cache[i]) {
						vmin = cache[i];
						imin = i;
					}
				}
				if (vmin == INT_MAX)
					break;
				cache[imin] = *++iruns[imin];
				++n;
			}
		//	printf("brute_cached: n=%zd\n", n);
		}
	}
	else {
		for (size_t pass = 0; pass < n_pass; ++pass) {
			for (size_t i = 0; i < num_runs; ++i) {
				iruns[i] = &runs[i][0];
			}
			size_t n = 0;
			for (;;) {
				size_t imin = 0;
				int vmin = *iruns[0];
				for (size_t i = 1; i < num_runs; ++i) {
					if (vmin > *iruns[i]) {
						vmin = *iruns[i];
						imin = i;
					}
				}
				if (vmin == INT_MAX)
					break;
				++iruns[imin];
				++n;
			}
		//	printf("brute_nocache: n=%zd\n", n);
		}
	}
	long t2 = pf.now();
#if 0
	int cache[num_runs];
	for (size_t pass = 0; pass < n_pass; ++pass) {
		for (size_t i = 0; i < num_runs; ++i) {
			iruns[i] = &runs[i][0];
			cache[i] =  runs[i][0];
		}
		size_t n = 0;
		for (;;) {
			/*
			size_t imin = num_runs-1;
			int vmin = cache[num_runs-1];
			switch (num_runs) {
			case 9:	if (vmin > cache[7]) vmin = cache[7], imin = 7;
			case 8:	if (vmin > cache[6]) vmin = cache[6], imin = 6;
			case 7:	if (vmin > cache[5]) vmin = cache[5], imin = 5;
			case 6:	if (vmin > cache[4]) vmin = cache[4], imin = 4;
			case 5:	if (vmin > cache[3]) vmin = cache[3], imin = 3;
			case 4:	if (vmin > cache[2]) vmin = cache[2], imin = 2;
			case 3:	if (vmin > cache[1]) vmin = cache[1], imin = 1;
			case 2:	if (vmin > cache[0]) vmin = cache[0], imin = 0;
			}
			*/
			size_t imin = 0;
			int vmin = cache[0];
			if (vmin > cache[1]) vmin = cache[1], imin = 1;
			if (vmin > cache[2]) vmin = cache[2], imin = 2;
			if (vmin > cache[3]) vmin = cache[3], imin = 3;
			if (vmin > cache[4]) vmin = cache[4], imin = 4;
			if (vmin > cache[5]) vmin = cache[5], imin = 5;
			if (vmin > cache[6]) vmin = cache[6], imin = 6;
			if (vmin > cache[7]) vmin = cache[7], imin = 7;
			if (vmin > cache[8]) vmin = cache[8], imin = 8;
			if (vmin > cache[9]) vmin = cache[9], imin = 9;
			if (vmin > cache[10]) vmin = cache[10], imin = 10;
			if (vmin > cache[11]) vmin = cache[11], imin = 11;
			if (vmin == INT_MAX)
				break;
			cache[imin] = *++iruns[imin];
			++n;
		}
	//	printf("brute_cached: n=%zd\n", n);
	}
#endif
	long t3 = pf.now();
	printf("sizeof(LoserTree)=%zd\n", sizeof(lt));
	printf("time: loser_tree=%f'ms brute_force=%f'ms unroll=%f'ms\n", pf.mf(t0,t1), pf.mf(t1,t2), pf.mf(t2,t3));
	return 0;
}

