#pragma once

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

#include "reactor.hpp"

namespace terark {

class Epoll_Reactor : public Reactor
{
	int epfd;
	int listenfd;

public:
	Epoll_Reactor(int port);

	void add(int fd, int evt);
	void modify(ConnectionStatePtr conn, int evt);
	void del(ConnectionStatePtr conn);

	void run();
	void handle_events(ConnectionStatePtr conn, int events);
};

} // namespace terark
