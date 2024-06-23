// test_replace_select_sort.cpp : Defines the entry point for the console application.
//

#include <string.h>
#include <stdlib.h>
#include <terark/replace_select_sort.hpp>
#include <terark/util/autoclose.hpp>
#include <terark/util/linebuf.cpp>
#include <terark/valvec.hpp>

int main(int argc, char* argv[]) {
	using namespace terark;
	size_t cap = 10;
	if (argc >= 2) {
		cap = atoi(argv[1]);
	}
	LineBuf line;
	auto reader = [&](int* val) {
		if (line.getline(stdin) > 0) {
			line.chomp();
			*val = atoi(line.p);
			return true;
		}
		return false;
	};
	size_t numWritten = 0;
	auto writerFactory = [&](size_t nth_run) {
		char fname[32];
		sprintf(fname, "out-%04zd", nth_run);
		printf("written = %9zd, nth_run: %s\n", numWritten, fname);
		Auto_fclose wf(fopen(fname, "w"));
		return unique_ptr_from_move_cons([&,f=std::move(wf)](int val) {
			numWritten++;
			fprintf(f, "%d\n", val);
		});
	};
	valvec<int> buf(cap, valvec_no_init());
	replace_select_sort(buf, reader, writerFactory);
	printf("written = %9zd, done\n", numWritten);
	return 0;
}

