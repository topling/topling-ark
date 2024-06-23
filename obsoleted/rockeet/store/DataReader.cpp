#include "DataReader.hpp"
#include <terark/io/FileStream.hpp>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sys/mman.h>
#include <aio.h>
#include <errno.h>

namespace terark { namespace rockeet {

//////////////////////////////////////////////////////////////////////////
DataSet::~DataSet()
{
//		m_reader->revoke(this);
}

size_t DataSet::totalSize() const
{
	size_t size = 0;
	for (const_iterator i = this->begin(); i != this->end(); ++i)
	{
		size += i->length;
	}
	return size;
}

//////////////////////////////////////////////////////////////////////////

IDataReader::~IDataReader()
{

}

void IDataReader::multiRead(DataSet& ds) const
{
	size_t totalSize = ds.totalSize();
	size_t pos = 0;
	DataBufferPtr bufp(totalSize);
	for (DataSet::iterator i = ds.begin(); i != ds.end(); ++i)
	{
		byte* p = bufp->data() + pos;
		read(i->u.offset, i->length, p);
		i->u.rawData = p;
	}
	ds.setBuf(bufp);
}

//////////////////////////////////////////////////////////////////////////

class MemDataReader : public IDataReader
{
	DataBufferPtr m_buf;

public:
	explicit MemDataReader(const std::string& fname)
	{
		FileStream fp(fname.c_str(), "rb");
		stream_position_t size = fp.size();
		DataBufferPtr(size).swap(m_buf);
		stream_position_t nRead = fp.read(m_buf->data(),size);
		if (nRead != size)
		{
			throw IOException("MemDataReader::MemDataReader");
		}
	}

	virtual void read(off_t offset, size_t size, void* pBuf) const
	{
		memcpy(pBuf, m_buf->data() + offset, size);
	}

	virtual void multiRead(DataSet& ds) const
	{
		for (DataSet::iterator i = ds.begin(); i != ds.end(); ++i)
		{
			i->u.rawData = m_buf->data() + i->u.offset;
		}
	}
};

//////////////////////////////////////////////////////////////////////////
#ifndef _WIN32
class MMapDataReader : public IDataReader
{
	int m_fd;
	byte* m_beg;
	uintptr_t m_size;

public:
	explicit MMapDataReader(const std::string& fname)
	{
		m_fd = ::open(fname.c_str(), O_RDONLY);
		if (-1 == m_fd) {
			throw OpenFileException(fname.c_str());
		}
		struct stat st;
		fstat(m_fd, &st);
		m_size = st.st_size;
		m_beg = (byte*)::mmap(NULL, m_size, PROT_READ, 0, m_fd, 0);
		if (0 == m_beg) {
			::close(m_fd);
			throw IOException(fname.c_str());
		}
	}
	~MMapDataReader()
	{
		::munmap(m_beg, m_size);
		::close(m_fd);
	}

	virtual void read(off_t offset, size_t size, void* pBuf) const
	{
		memcpy(pBuf, m_beg + offset, size);
	}

	virtual void multiRead(DataSet& ds) const
	{
		for (DataSet::iterator i = ds.begin(); i != ds.end(); ++i)
		{
			i->u.rawData = m_beg + i->u.offset;
		}
	}
};
//////////////////////////////////////////////////////////////////////////

class AioDataReader : public IDataReader
{
	int m_fd;

public:
	explicit AioDataReader(const std::string& fname)
	{
		m_fd = ::open(fname.c_str(), O_RDONLY);
		if (-1 == m_fd) {
			throw OpenFileException(fname.c_str());
		}
	}
	~AioDataReader()
	{
		::close(m_fd);
	}

	virtual void read(off_t offset, size_t size, void* pBuf) const
	{
		size_t nRead = ::pread(m_fd, pBuf, size, offset);
		if (nRead != size) {
			throw IOException("pread");
		}
	}

	virtual void multiRead(DataSet& ds) const
	{
		std::vector<aiocb>  aiov(ds.size());
		std::vector<aiocb*> paiov(ds.size());
		memset(&aiov[0], 0, sizeof(aiocb)*aiov.size());
		memset(&paiov[0], 0, sizeof(aiocb*)*paiov.size());
		size_t totalSize = ds.totalSize();
		DataBufferPtr bufp(totalSize);

		size_t pos = 0;
		for (ptrdiff_t i = 0; i != ds.size(); ++i)
		{
			aiocb* p = &aiov[i];
			p->aio_fildes = m_fd;
			p->aio_lio_opcode = LIO_READ;
			p->aio_buf = bufp->data() + pos;
			p->aio_nbytes = ds[i].length;
			p->aio_offset = ds[i].u.offset;
			paiov[i] = p;

			pos += ds[i].length;
		}
		int ret = lio_listio(LIO_WAIT, &paiov[0], paiov.size(), NULL);
		if (0 != ret) {
			throw IOException(errno, "lio_listio");
		}
		pos = 0;
		for (DataSet::iterator i = ds.begin(); i != ds.end(); ++i)
		{
			i->u.rawData = bufp->data() + pos;
			pos += i->length;
		}
		ds.setBuf(bufp);
	}
};
//////////////////////////////////////////////////////////////////////////

class pread_DataReader : public IDataReader
{
	int m_fd;

public:
	explicit pread_DataReader(const std::string& fname)
	{
		m_fd = ::open(fname.c_str(), O_RDONLY);
		if (-1 == m_fd) {
			throw OpenFileException(fname.c_str());
		}
	}
	~pread_DataReader()
	{
		::close(m_fd);
	}

	virtual void read(off_t offset, size_t size, void* pBuf) const
	{
		size_t nRead = ::pread(m_fd, pBuf, size, offset);
		if (nRead != size) {
			throw IOException("pread");
		}
	}

	virtual void multiRead(DataSet& ds) const
	{
		size_t totalSize = ds.totalSize();
		DataBufferPtr bufp(totalSize);

		size_t pos = 0;
		for (DataSet::iterator i = ds.begin(); i != ds.end(); ++i)
		{
			this->read(i->u.offset, i->length, bufp->data() + pos);
			pos += i->length;
		}
		pos = 0;
		for (DataSet::iterator i = ds.begin(); i != ds.end(); ++i)
		{
			i->u.rawData = bufp->data() + pos;
			pos += i->length;
		}
		ds.setBuf(bufp);
	}
};
//////////////////////////////////////////////////////////////////////////
#endif // #ifndef _WIN32

IDataReader* create(const std::string& policy, const std::string& fname)
{
	if (policy.compare("mem") == 0)
		return new MemDataReader(fname);
#ifndef _WIN32
	else if (policy.compare("aio") == 0)
		return new AioDataReader(fname);
	else if (policy.compare("pread") == 0)
		return new pread_DataReader(fname);
	else if (policy.compare("mmap") == 0)
		return new MMapDataReader(fname);
#endif
	return NULL;
}


} } // namespace terark::rockeet
