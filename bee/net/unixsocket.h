#pragma once

#include <bee/net/socket.h>
#include <bee/net/endpoint.h>

namespace bee::net::socket {
	bool u_enable();
	int  u_connect(fd_t s, const endpoint& ep);
	int  u_bind(fd_t s, const endpoint& ep);
}
