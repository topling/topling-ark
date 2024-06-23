#include <MapOutput.h>
#include <InputSplit.h>
#include <terark/trb_cxx.hpp>
#include <terark/io/IOException.hpp>

using namespace terark;

void flush(const trbstrmap<int>& cmap, MapOutput& output)
{
	for (trbstrmap<int>::const_iterator i = cmap.begin(); i != cmap.end(); ++i)
	{
	//	printf("WcMap::flush\n");
		assert(i.get_node() != NULL);
		const int* pcnt = &*i;
//		printf("%-40.*s %d\n", (int)cmap.klen(i), cmap.key(i), *pcnt);
		output.write(cmap.key(i), cmap.klen(i), pcnt, sizeof(int));
	}
}

int main(int argc, char* argv[])
try {
	MapContext context(argc, argv);
	TextInput input(&context);
	MapOutput output(&context);

	trbstrmap<int> cmap;
	try {
		std::string line;
		for (;;) {
			input.getline(line);
			printf("line: %s\n", line.c_str());
			const size_t delim = line.find('\t');
			if (line.npos != delim) {
				line[delim] = 0;
				cmap[line.c_str()]++;
				printf("linekey: %s\n", line.c_str());
			}
		}
	}
	catch (const EndOfFileException&) {

	}
	flush(cmap, output);

	return 0;
}
catch (const std::exception& e) {
	fprintf(stderr, "caught: %s\n", e.what());
	return 1;
}

