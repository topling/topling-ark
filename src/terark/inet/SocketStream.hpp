/* vim: set tabstop=4 : */
#pragma once

#include <stdio.h>
#include <terark/util/refcount.hpp>
#include <terark/io/IStream.hpp>
#include <terark/io/IOException.hpp>

#if defined(_WIN32) || defined(WIN32) || defined(_WIN64) || defined(WIN64)
#	if !defined(_WINSOCK2API_) && !defined(_WINSOCKAPI_)
#		include <WinSock2.h>
#	endif
#else
typedef int SOCKET;
#endif

namespace terark {

class TERARK_DLL_EXPORT SocketException : public IOException
{
public:
	explicit SocketException(const char* szMsg = "SocketException");
	explicit SocketException(int errCode, const char* szMsg = "SocketException");

	static int lastError();
};

class TERARK_DLL_EXPORT SocketStream : public RefCounter, public IDuplexStream
{
	DECLARE_NONE_COPYABLE_CLASS(SocketStream)
public:
	SocketStream(SOCKET socket, bool bAutoClose = true);
	~SocketStream();

public:
	size_t read(void* data, size_t length);
	size_t write(const void* data, size_t length);

	void flush() { }
	bool eof() const { return m_bEof; }

	size_t tellp() { return posp; }
	size_t tellg() { return posg; }

protected:
	virtual bool waitfor_again();

	::SOCKET socket;
	size_t posp; // sent size/pos
	size_t posg; // receive size/pos
	bool m_bEof; // for override IInputStream::eof
	bool m_bAutoClose;
};

class TERARK_DLL_EXPORT SocketAcceptor : public IAcceptor
{
	::SOCKET m_socket;
public:
	SocketAcceptor(const char* szBindAddr);
	SocketStream* accept();
};

TERARK_DLL_EXPORT SocketStream* ConnectSocket(const char* szServerAddr);

}
