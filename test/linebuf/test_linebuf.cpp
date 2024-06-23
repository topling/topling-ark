#define _CRT_SECURE_NO_WARNINGS
#include <terark/util/linebuf.hpp>
#include <terark/valvec.hpp>
#include <getopt.h>

int main(int argc, char* argv[]) {
	const char* delims = " ";
	for (;;) {
		int opt = getopt(argc, argv, "F:");
		switch (opt) {
			default:
				break;
			case -1:
				goto GetoptDone;
			case 'F':
				delims = optarg;
				break;
		}
	}
GetoptDone:
	printf("delims=%s:\n", delims);
	terark::LineBuf lb;
	terark::valvec<std::string> F;
	while (lb.getline(stdin) >= 0) {
		lb.chomp();
		lb.split_by_any(delims, &F);
		printf("lb.size() = %d\n", (int)lb.size());
		for (size_t i = 0; i < F.size(); ++i)
			printf(":%s:\t", F[i].c_str());
		printf("$\n");
	}
	return 0;
}

