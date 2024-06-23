//#include "HDFS_Stream.h"
#include <mapreduce/src/dfs.h>
#include <hdfs.h>
#include <terark/io/IOException.hpp>
#include <assert.h>

using namespace terark;

class HDFS_InputStream : public terark::IInputStream // terark::ISeekableInputStream
{
	void* m_fs;
	void* m_fp;
	bool  isEof;
public:
	HDFS_InputStream(void* hdfs, const char* pathName, uint64_t startOffset);
	~HDFS_InputStream()
	{
		hdfsCloseFile(m_fs, (hdfsFile)m_fp);
	}

	size_t read(void* data, size_t length);
	bool eof() const;
/*
	void seek(terark::stream_position_t pos);
	void seek(terark::stream_offset_t offset, int origin);
	terark::stream_position_t tell() const;
	terark::stream_position_t size() const;

	size_t pread(terark::stream_offset_t pos, void* buf, size_t length);
*/
};

class HDFS_OutputStream
//	: public terark::ISeekableOutputStream
	: public terark::IOutputStream
{
	void* m_fs;
	void* m_fp;
public:
	HDFS_OutputStream(void* hdfs, const char* pathName, int flags);
	size_t write(const void* data, size_t length);
	void flush();

	~HDFS_OutputStream()
	{
		hdfsCloseFile(m_fs, (hdfsFile)m_fp);
	}

//	void seek(terark::stream_position_t pos);
//	void seek(terark::stream_offset_t offset, int origin);
//	terark::stream_position_t tell() const;
//	terark::stream_position_t size() const;
//
//	size_t pwrite(terark::stream_offset_t pos, const void* buf, size_t length);
};

HDFS_InputStream::HDFS_InputStream(void* hdfs, const char* pathName, uint64_t startOffset)
{
	int   bufsize     = 8 * 1024;
	short replication = 0;
	tSize blockSize   = 0;

	hdfsFile fp = hdfsOpenFile(hdfs, pathName, O_RDONLY, bufsize, replication, blockSize);
	m_fp = fp;
	if (NULL == fp) {
		throw OpenFileException("HDFS_InputStream.hdfsOpenFile");
	}
	if (hdfsSeek(m_fs, fp, startOffset) == -1) {
		if (hdfsCloseFile(m_fs, fp) == -1) {
			throw OpenFileException("HDFS_InputStream.hdfsCloseFile");
		}
		throw OpenFileException("HDFS_InputStream.hdfsSeek");
	}
	isEof = false;
}

size_t HDFS_InputStream::read(void* data, size_t length)
{
	tSize nRead = hdfsRead(m_fs, (hdfsFile)m_fp, data, length);
	if (0 == nRead)
		isEof = true;
	return nRead;
}

bool HDFS_InputStream::eof() const
{
	return isEof;
}
/*
void HDFS_InputStream::seek(terark::stream_offset_t offset, int origin)
{
	fprintf(stderr, "unimplemented HDFS_InputStream::seek(offset, origin)");
}

void HDFS_InputStream::seek(terark::stream_position_t pos)
{
	if (hdfsSeek(m_fs, (hdfsFile)m_fp, pos) == -1) {

	}
}

terark::stream_position_t HDFS_InputStream::tell() const
{
	return hdfsTell(m_fs, (hdfsFile)m_fp);
}

terark::stream_position_t HDFS_InputStream::size() const
{
	fprintf(stderr, "unimplemented HDFS_InputStream::size()");
	abort();
	return 0;
}

size_t HDFS_InputStream::pread(terark::stream_offset_t pos, void* buf, size_t length)
{
	return hdfsPread(m_fs, (hdfsFile)m_fp, pos, buf, length);
}
*/
//
//------------------------------------------------------------------------------
//

HDFS_OutputStream::HDFS_OutputStream(void* hdfs, const char* pathName, int flags)
{
	int   bufsize     = 8 * 1024;
	short replication = 0;
	tSize blockSize   = 0;

	flags |= O_WRONLY|O_CREAT|O_APPEND;

	hdfsFile fp = hdfsOpenFile(hdfs, pathName, flags, bufsize, replication, blockSize);
	m_fp = fp;
	if (NULL == fp) {
		throw OpenFileException("HDFS_OutputStream.hdfsOpenFile");
	}
}

size_t HDFS_OutputStream::write(const void* data, size_t length)
{
	return hdfsWrite(m_fs, (hdfsFile)m_fp, data, length);
}

void HDFS_OutputStream::flush()
{
	if (hdfsFlush(m_fs, (hdfsFile)m_fp) == -1) {
		throw IOException("HDFS_OutputStream.hdfsFlush");
	}
}

/*
void HDFS_OutputStream::seek(terark::stream_offset_t offset, int origin)
{
	fprintf(stderr, "unimplemented HDFS_OutputStream::seek(offset, origin)");
}

void HDFS_OutputStream::seek(terark::stream_position_t pos)
{
	if (hdfsSeek(m_fs, (hdfsFile)m_fp, pos) == -1) {

	}
}

terark::stream_position_t HDFS_OutputStream::tell() const
{
	return hdfsTell(m_fs, (hdfsFile)m_fp);
}

terark::stream_position_t HDFS_OutputStream::size() const
{
	fprintf(stderr, "unimplemented HDFS_OutputStream::size()");
	abort();
	return 0;
}

size_t HDFS_OutputStream::pwrite(terark::stream_offset_t pos, const void* buf, size_t length)
{
	fprintf(stderr, "unimplemented HDFS_OutputStream::size()");
	abort();
	return 0;
//	return (size_t)hdfsPwrite(m_fs, (hdfsFile)m_fp, pos, buf, length);
}
*/

#ifdef _MSC_VER
#  define DLL_EXPORT __declspec(dllexport)
#else
#  define DLL_EXPORT
#endif

class HDFS_FileSystem : public FileSystem
{
	void* m_fs;
	char* m_namenode;
public:
	HDFS_FileSystem(const char* host, int port, const char* user, const char* groups[], int ngroup)
	{
		m_fs = hdfsConnectAsUser(host, port, user, groups, ngroup);
		if (NULL == m_fs) {
			throw OpenFileException("hdfsConnectAsUser");
		}
		if (asprintf(&m_namenode, "%s@%s:%d", user, host, port) == -1) {
			perror("asprintf@HDFS_FileSystem::HDFS_FileSystem");
			exit(1);
		}
	}

	~HDFS_FileSystem()
	{
		assert(NULL != m_fs);
		hdfsDisconnect(m_fs);
		free(m_namenode);
	}

	IInputStream* openInput(const char* pathname, int64_t startOffset)
	{
		return new HDFS_InputStream(m_fs, pathname, startOffset);
	}

	HDFS_OutputStream* openOutput(const char* pathname, int flags)
	{
		return new HDFS_OutputStream(m_fs, pathname, flags);
	}

	size_t getBlockSize()
	{
		return hdfsGetDefaultBlockSize(m_fs);
	}

    char*** getHosts(const char* path, long long start, long long length)
    {
        return hdfsGetHosts(m_fs, path, start, length);
    }

    void freeHosts(char*** blocksHosts)
    {
        hdfsFreeHosts(blocksHosts);
    }

	const char* namenode()
	{
		return this->m_namenode;
	}
};

DLL_EXPORT FileSystem* createFileSystem(const char* host, int port, const char* user, const char* groups[], int ngroup)
{
	return new HDFS_FileSystem(host, port, user, groups, ngroup);
}

