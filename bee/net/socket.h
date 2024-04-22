#pragma once

#include <bee/net/fd.h>

namespace bee::net {
    struct endpoint;
}

namespace bee::net::socket {
    enum class protocol {
        tcp,
        udp,
        unix,
        tcp6,
        udp6,
    };

    enum class shutdown_flag {
        both,
        read,
        write,
    };

    enum class status {
        success,
        wait,
        failed,
    };

    enum class recv_status {
        success,
        wait,
        failed,
        close,
    };

    enum class option {
        reuseaddr,
        sndbuf,
        rcvbuf,
    };

    enum class fd_flags {
        none,
        nonblock,
    };

    bool initialize() noexcept;
    fd_t open(protocol protocol, fd_flags flags = fd_flags::nonblock) noexcept;
    bool pair(fd_t sv[2], fd_flags flags = fd_flags::nonblock) noexcept;
    bool pipe(fd_t sv[2], fd_flags flags = fd_flags::nonblock) noexcept;
    bool close(fd_t s) noexcept;
    bool shutdown(fd_t s, shutdown_flag flag) noexcept;
    bool setoption(fd_t s, option opt, int value) noexcept;
    bool bind(fd_t s, const endpoint& ep) noexcept;
    bool listen(fd_t s, int backlog) noexcept;
    status connect(fd_t s, const endpoint& ep) noexcept;
    status accept(fd_t s, fd_t& newfd, fd_flags flags = fd_flags::nonblock) noexcept;
    recv_status recv(fd_t s, int& rc, char* buf, int len) noexcept;
    status send(fd_t s, int& rc, const char* buf, int len) noexcept;
    status recvfrom(fd_t s, int& rc, endpoint& ep, char* buf, int len) noexcept;
    status sendto(fd_t s, int& rc, const char* buf, int len, const endpoint& ep) noexcept;
    bool getpeername(fd_t s, endpoint& ep) noexcept;
    bool getsockname(fd_t s, endpoint& ep) noexcept;
    bool errcode(fd_t s, int& err) noexcept;
    fd_t dup(fd_t s) noexcept;
}
