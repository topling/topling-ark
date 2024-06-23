

#include "HDFS_Stream.h"
#include <hdfs.h>
#include <terark/io/IOException.hpp>

using namespace terark;

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

bool HDFS_InputStream::eof()
{
	return isEof;
}

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

