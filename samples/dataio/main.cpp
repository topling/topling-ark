#include <terark/io/MemStream.hpp>
#include <terark/io/FileStream.hpp>
#include <terark/io/StreamBuffer.hpp>

int main(int argc, char* argv[])
{
	using namespace terark;
	size_t len;
	printf("test--AutoGrownMemIO::printf\n");
	AutoGrownMemIO mio;
	len = mio.printf("%s %d\n", argv[0], argc);
	assert(mio.tell() == len);
	printf("%s\n", mio.buf());

	printf("test--StreamBuffer::printf\n");
	FileStream file;
	file.attach(stdout);
	OutputBuffer sb(&file);
	sb.set_bufsize(1);
	for (int i = 0; i < 10; ++i) {
		len = sb.printf("%s %d\n", argv[0], argc);
		assert(mio.tell() == len);
	}
	sb.flush();
	file.detach();

	return 0;
}

