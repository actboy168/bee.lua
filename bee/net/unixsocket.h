#pragma once

#include <bee/net/socket.h>
#include <bee/net/endpoint.h>

namespace bee::net::socket {
    status u_connect(fd_t s, const endpoint& ep);
    status u_bind(fd_t s, const endpoint& ep);
}
