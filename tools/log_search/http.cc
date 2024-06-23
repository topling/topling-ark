#include "http.hpp"

#include <time.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/signalfd.h>
#include <unistd.h>
#include <signal.h>

namespace terark {

HttpRequestHandlerFactory::~HttpRequestHandlerFactory() {}

void HttpRequestHandler::onHeaderEnd(HttpRequest*, HttpServer* server) {}
void HttpRequestHandler::onBodyLine(HttpRequest*, HttpServer* server, fstring line) {}
void HttpRequestHandler::onPeerClose(HttpRequest*, HttpServer* server) {}
HttpRequestHandler::~HttpRequestHandler() {}

HttpRequest::HttpRequest(int sockfd)
  : NetBuffer(sockfd)
{
	m_httpHeaderEnded = false;
	m_headerLineCount = 0;
	m_isClosed = false;
}

HttpRequest::~HttpRequest() {
	if (m_isClosed)
		m_fd = -1;
}

void HttpRequest::closefd() {
	assert(m_fd >= 0);
	::close(m_fd);
	m_isClosed = true;
}

void HttpRequest::onOneLine(fstring line, void* context) {
	HttpServer* server = (HttpServer*)context;
	if (m_httpHeaderEnded) {
		assert(m_handler.get() != NULL);
		m_handler->onBodyLine(this, server, line);
	}
	else {
		this->parseHeaderLine(server, line);
		assert(m_handler.get() != NULL);
		if (m_httpHeaderEnded) {
			m_handler->onHeaderEnd(this, server);
		}
	}
}

void HttpRequest::onQueryLine(HttpServer* server, fstring line) {
	valvec<fstring> F(8, valvec_reserve());
	line.split(' ', &F);
	if (F.size() == 3) {
		m_method  = F[0].str();
		m_uri     = F[1].str();
		m_version = F[2].str();
		auto ask = std::find(m_uri.begin(), m_uri.end(), '?');
		if (m_uri.end() == ask) {
			m_qry.clear();
			m_baseURI = m_uri;
		}
		else {
			m_baseURI = fstring(m_uri.data(), ask - m_uri.begin());
			++ask;
			m_qry.assign(&*ask, m_uri.end() - ask);
		}
		fstring(m_qry).split('&', &F);
		for (size_t i = 0; i < F.size(); ++i) {
			fstring a = F[i];
			const char* eq = std::find(a.begin(), a.end(), '=');
			fstring& val = m_args[fstring(a.begin(), eq)];
			if (a.end() != eq) {
				val = fstring(eq+1, a.end());
				*(char*)a.end() = '\0';
			}
		}
		m_handler.reset(server->createHandler(this, m_baseURI));
		if (!m_handler) {
			THROW_STD(invalid_argument,
					"Not Found handler, uri: %s\n",
					m_uri.c_str());
		}
	}
	else {
		THROW_STD(invalid_argument,
				"parseHttpHeaderMethod: F.size=%ld: %.*s\n",
				(long)F.size(), line.ilen(), line.data());
	}
}

void HttpRequest::parseHeaderLine(HttpServer* server, fstring line) {
	line.trim();
	long lineno = NetBuffer::m_lineCount;
	if (0 == lineno) {
		onQueryLine(server, line);
	}
	else if (line.empty()) {
		m_httpHeaderEnded = true;
//		fprintf(stderr, "Found \\r\\n after HttpHeader\n");
	}
	else {
		const char* colon = std::find(line.begin(), line.end(), ':');
		if (line.end() == colon) {
			THROW_STD(invalid_argument,
					"Invalid HttpHeader: lineno=%ld: %.*s\n",
					lineno, line.ilen(), line.data());
		}
		else {
			fstring key(line.begin(), colon);
			fstring val(colon+1, line.end());
			key.trim();
			val.trim();
			if (!m_header.insert_i(key).second) {
				THROW_STD(invalid_argument,
					"parseHttpHeader(lineno=%ld): duplicate header: %.*s\n",
					lineno, line.ilen(), line.data());
			}
			m_headerValues.push_back(val, '\0');
		}
	}
}

HttpServer::HttpServer() {
	m_epoll_fd = -1;
	m_listen_fd = -1;
	m_signal_fd = -1;
	m_port = 0;
	m_num_fd_in_epoll = 0;
	m_stopping = false;
}

HttpServer::~HttpServer() {
	if (m_epoll_fd >= 0) close(m_epoll_fd);
	if (m_listen_fd >= 0) close(m_listen_fd);
	if (m_signal_fd >= 0) close(m_signal_fd);
}

HttpRequestHandler* HttpServer::createHandler(HttpRequest* r, fstring baseURI) {
	HttpRequestHandlerFactory* factory = m_uriHandlerMap[baseURI];
	if (factory) {
		return factory->create(r);
	}
	return NULL;
}

int HttpServer::start(int port) {
	m_epoll_fd = epoll_create(1);
	if (m_epoll_fd < 0) {
		fprintf(stderr, "ERROR: epoll_create(1) = %m\n");
		return 3;
	}
	m_listen_fd = socket(AF_INET, SOCK_STREAM|SOCK_NONBLOCK, IPPROTO_TCP);
	if (m_listen_fd < 0) {
		fprintf(stderr, "ERROR: socket(tcp) = %m\n");
		return 3;
	}
	struct sockaddr_in iaddr;
	memset(&iaddr, 0, sizeof(iaddr));
	iaddr.sin_family = AF_INET;
	iaddr.sin_addr.s_addr = 0;
	iaddr.sin_port = htons(m_port);
	if (::bind(m_listen_fd, (sockaddr*)&iaddr, sizeof(iaddr)) < 0) {
		fprintf(stderr, "ERROR: bind(listenfd) = %m\n");
		return 3;
	}
	int backlog = 128;
	if (listen(m_listen_fd, backlog) < 0) {
		fprintf(stderr, "ERROR: socket(tcp) = %m\n");
		return 3;
	}
	const int MAX_EVENTS = 16;
	epoll_event ev, evList[MAX_EVENTS];
	ev.data.u64 = m_listen_fd;
	ev.events = EPOLLIN | EPOLLET;
	if (epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, m_listen_fd, &ev) < 0) {
		fprintf(stderr, "ERROR: epoll_ctl(add listen fd) = %m\n");
		return 3;
	}
	sigset_t sigmask;
	sigemptyset(&sigmask);
	sigaddset(&sigmask, SIGINT);
	sigaddset(&sigmask, SIGQUIT);
	if (sigprocmask(SIG_BLOCK, &sigmask, NULL) < 0) {
		perror("sigprocmask");
		return 3;
	}
	m_signal_fd = signalfd(-1, &sigmask, SFD_NONBLOCK);
	if (m_signal_fd < 0) {
		perror("signalfd");
		return 3;
	}
	ev.data.u64 = m_signal_fd;
	ev.events = EPOLLIN | EPOLLET;
	if (epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, m_signal_fd, &ev) < 0) {
		fprintf(stderr, "ERROR: epoll_ctl(add signal fd) = %m\n");
		return 3;
	}
	fprintf(stderr, "Server started at port: %d\n", m_port);
	m_stopping = false;
	for (;;) {
		int timeout_msec = 2000;
		int nEv = epoll_wait(m_epoll_fd, evList, MAX_EVENTS, timeout_msec);
		fprintf(stderr
				, "epoll_wait.numEvents = %d | m_num_fd_in_epoll = %d\n"
				, nEv, m_num_fd_in_epoll);
		if (nEv < 0) {
			if (EINTR == errno) {
				if (0 == m_num_fd_in_epoll && m_stopping) {
					fprintf(stderr, "Server Graceful Completed\n");
					break;
				}
				fprintf(stderr, "epoll_wait timeout, continue...\n");
				continue;
			}
			else {
				fprintf(stderr, "ERROR: epoll_wait() = %m\n");
				return 3;
			}
		}
		if (0 == nEv && m_stopping && 0 == m_num_fd_in_epoll) {
			fprintf(stderr, "Server Graceful Completed\n");
			break;
		}
		for (int i = 0; i < nEv; ++i) {
//---------------------------------------------------------------------
if (uint64_t(m_listen_fd) == evList[i].data.u64) {
	for (;;) {
		struct sockaddr_in addr;
		socklen_t addrSize = 0;
		memset(&addr, 0, sizeof(addr));
	//	int clientfd = accept4(m_listen_fd, (sockaddr*)&addr, &addrSize, SOCK_NONBLOCK);
		int clientfd = accept(m_listen_fd, (sockaddr*)&addr, &addrSize);
		if (clientfd < 0) {
			perror("accept(clientfd)");
			break;
		}
		if (m_stopping) {
			dprintf(clientfd, "Server is stopping\n");
			close(clientfd);
		}
		else {
			HttpRequest* r = new HttpRequest(clientfd);
			ev.events = EPOLLIN | EPOLLET;
			ev.data.u64 = uint64_t(r);
			if (epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, clientfd, &ev) < 0) {
				perror("epoll_ctl(add clientfd)");
			} else {
				m_num_fd_in_epoll++;
			}
		}
	}
}
else if (uint64_t(m_signal_fd) == evList[i].data.u64) {
	for (;;) {
		signalfd_siginfo fdsi;
		long s = ::read(m_signal_fd, &fdsi, sizeof(signalfd_siginfo));
		if (s != sizeof(struct signalfd_siginfo)) {
			if (EAGAIN != errno) {
				fprintf(stderr,
					"read signalfd: read=%ld readed=%ld errno=(%d): %m\n",
					long(sizeof(signalfd_siginfo)), s, errno);
			}
			break;
		}
		if (fdsi.ssi_signo == SIGINT) {
			fprintf(stderr, "Got SIGINT\n");
			m_stopping = true;
		} else if (fdsi.ssi_signo == SIGQUIT) {
			fprintf(stderr, "Got SIGQUIT\n");
			m_stopping = true;
		} else {
			fprintf(stderr, "Read unexpected signal\n");
			return 3;
		}
	}
}
else {
	HttpRequest* r = (HttpRequest*)(evList[i].data.ptr);
	try {
		int err = r->getNetLines(this);
		if (0 == err || r->m_isClosed) {
			epoll_ctl(m_epoll_fd, EPOLL_CTL_DEL, r->fd(), NULL);
			m_num_fd_in_epoll--;
			try {
				if (r->m_handler)
					r->m_handler->onPeerClose(r, this);
			}
			catch (...) {
			}
			delete r;
		}
	}
	catch (const std::exception& ex) {
		fprintf(stderr
			, "Exception: ex.what=%s; deleting request: uri: %s\n"
			, ex.what(), r->m_uri.c_str()
			);
		epoll_ctl(m_epoll_fd, EPOLL_CTL_DEL, r->fd(), NULL);
		m_num_fd_in_epoll--;
		delete r;
	}
}
//---------------------------------------------------------------------
		}
	} // epoll loop
	return 0;
}

} // namespace terark

