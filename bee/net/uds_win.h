#pragma once

#include <bee/net/fd.h>
#include <bee/net/socket.h>

namespace bee::net::socket {
    status u_connect(fd_t s, const endpoint& ep) noexcept;
    bool u_bind(fd_t s, const endpoint& ep);
    bool u_support() noexcept;
    fd_t u_createSocket(int af, int type, int protocol, fd_flags fd_flags) noexcept;
}
