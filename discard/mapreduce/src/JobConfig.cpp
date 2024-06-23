#include <terark/io/DataIO.hpp>
#include "JobConfig.h"
#include "dfs.h"
#include <terark/trb_cxx.hpp>
#include <terark/io/IOException.hpp>
#include <terark/io/StreamBuffer.hpp>
#include <boost/current_function.hpp>
#include <fstream>

#include <dlfcn.h>

using namespace terark;

typedef terark::trbstrmap<std::vector<std::string> >  pmap_t;
class JobConfig_impl
{
public:
	pmap_t pmap;
	int lineno;
};

//////////////////////////////////////////////////////////////////////////////

JobConfig::JobConfig()
{
	impl = new JobConfig_impl;
}

JobConfig::~JobConfig()
{
	delete impl;
}

void JobConfig::loadpkv(const std::string& filename)
{
	fprintf(stderr, "loadkpv: %s ...\n", filename.c_str());
	std::ifstream file(filename.c_str());
	if (!file.is_open()) {
		throw OpenFileException(filename.c_str());
	}
	impl->lineno = 1;
	std::string line;
	while (std::getline(file, line)) {
		if ('#' == line[0]) {
			// comment
			continue;
		}
		parseLine(line.data(), line.size());
		impl->lineno++;
	}
	fprintf(stderr, "loadkpv: %s successed, lines=%d\n", filename.c_str(), impl->lineno);
	impl->lineno = -1;
}

void JobConfig::loadxml(const std::string& filename)
{
	throw std::invalid_argument(BOOST_CURRENT_FUNCTION);
}

void JobConfig::parseValues(const std::string& key, const char* valueBeg, const char* valueEnd)
{
	const char quote = valueBeg[0];
	if ('"' == quote || '\'' == quote) {
		if (quote != valueEnd[-1]) {
			char buf[128];
			sprintf(buf, "syntax error at line: %d, missing close quote %c", impl->lineno, quote);
		//	throw std::invalid_argument(buf);
			fprintf(stderr, "%s\n", buf);
			exit(1);
		}
		std::string val(valueBeg + 1, valueEnd - 1);
		impl->pmap[key].push_back(val);
	}
	else {
		// maybe ',' seperated values
		std::vector<std::string>& vals = impl->pmap[key];
		const char* currVal = valueBeg;
		for (;;) {
			const char* nextVal = (const char*)memchr(currVal, ',', valueEnd - currVal);
			if (NULL == nextVal) {
				if (currVal < valueEnd) { // only non-empty value ?
					vals.push_back(currVal);
				}
				break;
			}
			else {
				if (currVal < nextVal) { // only non-empty value ?
					std::string val(currVal, nextVal);
					vals.push_back(val);
				}
				currVal = nextVal + 1;
			}
		}
	}
}

void JobConfig::parseValues(const std::string& key, const char* valueBeg)
{
	parseValues(key, valueBeg + strlen(valueBeg));
}

void JobConfig::parseLine(const char* line, int length)
{
	const char* keyEnd = (const char*)memchr(line, '=', length);
	if (NULL == keyEnd) {
		// not found value, treat as bool true
		std::vector<std::string>& vals = impl->pmap[line];
		if (vals.empty()) {
			vals.push_back("true");
		}
	   	else if (vals.size() == 1 && (vals[0] == "false" || vals[0] == "0")) {
			vals[0] = "true";
		}
		else {
			// do nothing
		}
	}
	else {
		std::string key(line, keyEnd);
		parseValues(key, keyEnd + 1, line + length);
	}
}

void JobConfig::parseCmd(int argc, char* argv[])
{
	for (int i = 1; i < argc; ++i) {
		if (0) {
		}
		else if (strcmp(argv[i], "-D") == 0) {
			++i;
			parseLine(argv[i], strlen(argv[i]));
		}
		else if (strcmp(argv[i], "--conf") == 0) {
			loadpkv(argv[++i]);
		}
		else if (strcmp(argv[i], "--pkvconf") == 0) {
			loadpkv(argv[++i]);
		}
		else if (strcmp(argv[i], "--xmlconf") == 0) {
			loadxml(argv[++i]);
		}
		else if (strcmp(argv[i], "-input") == 0) {
			parseValues("MapInput.path", argv[++i]);
		}
		else if (strcmp(argv[i], "-output") == 0) {
			parseValues("reduce.output.path", argv[++i]);
		}
		else if (strcmp(argv[i], "-fs") == 0) {
			parseValues("dfs.namenode", argv[++i]);
		}
		else if (strcmp(argv[i], "-file") == 0) {
			parseValues("task.file", argv[++i]);
		}
		else if (strcmp(argv[i], "-numReduceTasks") == 0) {
			parseValues("task.file", argv[++i]);
		}
		else if (strcmp(argv[i], "-reducedebug") == 0) {
			parseValues("reduce.debug", argv[++i]);
		}
		else if (strcmp(argv[i], "-mapdebug") == 0) {
			parseValues("map.debug", argv[++i]);
		}
	}
}

const std::vector<std::string>& JobConfig::getMulti(const char* property) const
{
	pmap_t::const_iterator iter = impl->pmap.find(property);
	if (impl->pmap.end() == iter) {
		static const std::vector<std::string> empty;
		return empty;
	}
	return *iter;
}

void JobConfig::add(const char* property, const std::string& value)
{
	std::vector<std::string>& vec = impl->pmap[property];
	vec.push_back(value);
}

const std::string JobConfig::get(const char* property, const char* defaultValue) const
{
	pmap_t::const_iterator iter = impl->pmap.find(property);
	if (impl->pmap.end() == iter) {
		return defaultValue;
	}
	if (iter->empty()) {
		fprintf(stderr, "key=%s, has no values\n", property);
		exit(1);
	}
	return iter->front();
}

void JobConfig::set(const char* property, const std::string& value)
{
	std::vector<std::string>& vec = impl->pmap[property];
	assert(vec.size() <= 1);
	vec[0] = value;
}

long long JobConfig::getll(const char* property, long long defaultValue) const
{
	pmap_t::const_iterator iter = impl->pmap.find(property);
	if (impl->pmap.end() == iter) {
		return defaultValue;
	}
	assert(!iter->empty());
	return strtoll(iter->front().c_str(), NULL, 10);
}

void JobConfig::setll(const char* property, long long value)
{
	std::vector<std::string>& vec = impl->pmap[property];
	assert(vec.size() <= 1);
	char buf[32];
	snprintf(buf, 32, "%lld", value);
	vec[0] = buf;
}

void JobConfig::load(InputBuffer& ibuf)
{
	DATA_IO_DEFINE_REF(PortableDataInput<InputBuffer>, input, ibuf);
	input >> impl->pmap;
}

void JobConfig::save(OutputBuffer& obuf) const
{
	DATA_IO_DEFINE_REF(PortableDataOutput<OutputBuffer>, output, obuf);
	output << impl->pmap;
}

FileSystem* JobConfig::getFileSystem()
{
	const std::string namenode = get("dfs.namenode", "local");

	FileSystem* fs = NULL;

	if (namenode == "local") {
		fs = createLocalFileSystem();
		if (NULL == fs) {
			fprintf(stderr, "createLocalFileSystem failed\n");
			exit(1);
		}
	}
	else {
		const std::string libfile = get("dfs.libfile", "");
		if (libfile.empty()) {
			fprintf(stderr, "dfs.libfile is empty\n");
		//	throw std::invalid_argument("dfs.libfile is empty");
			exit(1);
		}
		void* dll = dlopen(libfile.c_str(), RTLD_NOW);
		if (NULL == dll) {
			fprintf(stderr, "can not load dfs.libfile '%s', because: %s\n", libfile.c_str(), dlerror());
			exit(1);
		}
		CreateFileSystemFP createfs = (CreateFileSystemFP)dlsym(dll, "createFileSystem");
		if (NULL == createfs) {
			fprintf(stderr, "can't find function 'createFileSystem' in dfs.libfile '%s'\n", libfile.c_str());
		}
		fprintf(stderr, "load createFileSystem@%s successed\n", libfile.c_str());

		const char* envuser = getenv("USER");
		if (NULL == envuser || '\0' == envuser[0]) {
			envuser = getenv("USERNAME");
		}
		if (NULL == envuser || '\0' == envuser[0]) {
			envuser = "root";
		}
		const std::string user = get("dfs.user", envuser);
		const int port = getll("dfs.namenode.port", 0);
		const std::vector<std::string>& vgroups = getMulti("dfs.groups");
		std::vector<const char*> groups(vgroups.size());
		for (int i = 0; i < (int)vgroups.size(); ++i) {
			groups[i] = vgroups[i].c_str();
		}
		fs = createfs(namenode.c_str(), port, user.c_str(), &groups[0], groups.size());
		if (NULL == fs) {
			fprintf(stderr, "createFileSystem failed\n\tnamenode=%s, user=%s\n", namenode.c_str(), user.c_str());
			exit(1);
		}
	}

	return fs;
}

