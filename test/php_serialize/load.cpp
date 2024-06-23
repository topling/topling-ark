#include <terark/ptree.hpp>
#include <terark/objbox.hpp>
#include <stdio.h>

using namespace terark::objbox;

int main()
{
	std::string buf;
	buf.reserve(1);
	for (;;) {
		size_t oldsize = buf.size();
		buf.resize(std::max<size_t>(buf.size()*2, 1));
		ssize_t nread = ::read(STDIN_FILENO, &buf[oldsize], buf.size()-oldsize);
		if (nread > 0)
			buf.resize(oldsize + nread);
		else
			break;
	}
	const char* beg = buf.data();
	const char* end = buf.data() + buf.size();
	fprintf(stderr, "loading...\n");
	boost::intrusive_ptr<obj> p(php_load(&beg, end));
	fprintf(stderr, "loaded\n");
	std::string buf2;
	php_save(p.get(), &buf2);
	buf2.append("\n");
	::write(STDOUT_FILENO, buf2.data(), buf2.size());
	return 0;
}

