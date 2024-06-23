#ifndef __terark_http_hpp__
#define __terark_http_hpp__

#include "NetBuffer.hpp"

namespace terark {

class HttpRequest;
class HttpRequestHandler;
class HttpServer;

class HttpRequestHandlerFactory {
public:
	virtual ~HttpRequestHandlerFactory();
	virtual HttpRequestHandler* create(HttpRequest*) = 0;
};
template<class Handler>
class DefaultHandlerFactory : public HttpRequestHandlerFactory {
public:
	HttpRequestHandler*
	create(HttpRequest* r) override { return new Handler(r); }
};

class HttpRequestHandler {
public:
	~HttpRequestHandler();
	virtual void onHeaderEnd(HttpRequest*, HttpServer* server);
	virtual void onBodyLine(HttpRequest*, HttpServer* server, fstring line);
	virtual void onPeerClose(HttpRequest*, HttpServer* server);
};

class HttpRequest : public NetBuffer {
public:
	hash_strmap<> m_header;
	hash_strmap<> m_argNames;
	hash_strmap<fstring> m_args;
	wfstrvec m_argValues;
	fstrvec m_headerValues;
	std::string m_method;
	std::string m_uri;
	std::string m_qry;
	std::string m_version;
	std::unique_ptr<HttpRequestHandler> m_handler;
	fstring m_baseURI;
	bool m_isClosed;
	bool m_httpHeaderEnded;
	long m_headerLineCount; // including \r\n\r\n line

	long long headerLineCount() const { return m_headerLineCount; }
	long long bodyLineCount() const { return m_lineCount - m_headerLineCount; }

	void closefd();

	void onOneLine(fstring line, void* context) override;

	explicit HttpRequest(int sockfd);
	virtual ~HttpRequest();

	const std::string& queryString() const { return m_qry; }

	fstring arg(fstring name) const { return m_args[name]; }
	bool argExists(fstring name) const { return m_args.exists(name); }

	void parseHeaderLine(HttpServer*, fstring line);

	const std::string& method() const { return m_method; }
	const std::string& uri() const { return m_uri; }
	const std::string& version() const { return m_version; }
	fstring baseURI() const { return m_baseURI; }

	virtual void onQueryLine(HttpServer* server, fstring line);
};

class HttpServer {
protected:
	int m_epoll_fd;
	int m_listen_fd;
	int m_signal_fd;
	int m_port;
	int m_num_fd_in_epoll;
	bool m_stopping;

public:
	hash_strmap_p<HttpRequestHandlerFactory> m_uriHandlerMap;
//	gold_hash_map_p<HttpRequest*, HttpRequest> m_requests;

	HttpServer();
	~HttpServer();

	HttpRequestHandler* createHandler(HttpRequest* r, fstring baseURI);
	int start(int port);
};

} // namespace terark

#endif // __terark_http_hpp__

