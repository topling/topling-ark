#ifndef __mapreduce_HDFS_Stream_h__
#define __mapreduce_HDFS_Stream_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
# pragma warning(disable: 4127)
#endif

#include <terark/io/IStream.hpp>

class HDFS_InputStream : public terark::IInputStream
{
	void* m_fs;
	void* m_fp;
	bool  isEof;
public:
	HDFS_InputStream(void* hdfs, const char* pathName, uint64_t startOffset);
	size_t read(void* data, size_t length);
	bool eof();
};

class HDFS_OutputStream : public terark::IOutputStream
{
	void* m_fs;
	void* m_fp;
public:
	HDFS_OutputStream(void* hdfs, const char* pathName, int flags);
	size_t write(const void* data, size_t length);
	void flush();
};

#endif //__mapreduce_HDFS_Stream_h__

