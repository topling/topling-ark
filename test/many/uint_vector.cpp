#include <stdio.h>
#include <terark/int_vector.hpp>

using namespace terark;

int main(int argc, char* argv[]) {
	if (argc < 3) {
		fprintf(stderr, "usage %s max_val max_num\n", argv[0]);
		return 0;
	}
	size_t max_val = atoi(argv[1]);
	size_t max_num = atoi(argv[2]);
	valvec<size_t> v1;
	for (size_t i = 0; i < max_num; ++i) {
		v1.push_back(rand() % (max_val + 1));
	}
	UintVector uv;
	uv.build_from(v1);
	printf("v1.size()=%zd\n", v1.size());
	printf("v1 bytes is : %zd\n", sizeof(size_t) * v1.size());
	printf("uv.uintbits()=%zd uv.mem_size()=%zd\n", uv.uintbits(), uv.mem_size());
	for (size_t i = 0; i < v1.size(); ++i) printf(" %zd", v1[i]);
	printf("\n");
	for (size_t i = 0; i < uv.size(); ++i) printf(" %zd", uv[i]);
	printf("\n");
	return 0;
}

