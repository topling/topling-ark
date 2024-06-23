#pragma once

#include <terark/util/refcount.hpp>
#include <boost/intrusive_ptr.hpp>
#include <boost/coro>

namespace terark {

class ConnectionState : public RefCounter
{
public:
	int fd;
	void* coro;

	ConnectionState(int fd);
};

typedef boost::intrusive_ptr<ConnectionState> ConnectionStatePtr;

class Reactor
{
	int epfd;
	void* main_coro;
public:
	Reactor();
};

} // namespace terark
