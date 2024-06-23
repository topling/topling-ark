#include <terark/hash_strmap.hpp>

int main() {
	for(int i = 1; i < 200; ++i) {
		int k = (int)terark::__hsm_align_pow2(i);
		//printf("%03d : %03d : %03X\n", i, k, k);
	}
	return 0;
}

