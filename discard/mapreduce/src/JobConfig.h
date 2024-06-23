#ifndef __mapreduce_JobConfig_h__
#define __mapreduce_JobConfig_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
# pragma warning(disable: 4127)
#endif

#include <string>
#include <vector>
#include <terark/io/StreamBuffer.hpp>
#include "config.hpp"

class JobConfig_impl;
class FileSystem;
class MAP_REDUCE_DLL_EXPORT JobConfig
{
	JobConfig_impl* impl;
protected:
	void parseValues(const std::string& key, const char* valueBeg, const char* valueEnd);
	void parseValues(const std::string& key, const char* valueBeg);
	void parseLine(const char* line, int length);

public:
	JobConfig();
	~JobConfig();

	void parseCmd(int argc, char* argv[]);

	void loadpkv(const std::string& filename);
	void loadxml(const std::string& filename);

	const std::vector<std::string>& getMulti(const char* property) const;
	void  add(const char* property, const std::string& value);

	const std::string get(const char* property, const char* defaultValue) const;
	void  set(const char* property, const std::string& value);

	long long getll(const char* property, long long defaultValue) const;
	void setll(const char* property, long long value);

	void load(terark:: InputBuffer& ibuf);
	void save(terark::OutputBuffer& obuf) const;

	FileSystem* getFileSystem();
};

#endif //__mapreduce_JobConfig_h__

