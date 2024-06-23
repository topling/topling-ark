#include "MapOutput.h"

int main(int argc, char* argv[])
{
	MapContext context(argc, argv);
	char delim = '\t';
	for (int i = 1; i < argc; ++i) {
		if (strcmp(argv[i], "--delim") == 0) {
			delim = argv[++i][0];
		}
	}
	TextInput input(&context);
	MapOutput output(&context);
	InputBuffer ib(input.get());
	try {
		std::string line;
		for (;;) {
			ib.getline(line, INT_MAX);
			size_t klen = line.find(delim);
			if (line.npos == klen) {
				output->write(line.data(), line.size(), NULL, 0);
			}
		   	else {
				output->write(line.data(), klen, line.data() + klen + 1, line.size() - klen - 1);
			}
		}
	}
	catch (const EndOfFileException& e) {
	}
	return 0;
}

