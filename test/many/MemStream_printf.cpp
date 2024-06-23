#include <terark/io/MemStream.hpp>

int main(int argc, char* argv[]) {
	std::string str(100, 'A');
	terark::AutoGrownMemIO mem;
	mem.printf("%s\n", str.c_str());
	printf("%s", mem.begin());
	return 0;
}

