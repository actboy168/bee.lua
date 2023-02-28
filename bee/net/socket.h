#pragma once

#if defined _WIN32
#include <stdint.h>
#endif

namespace bee::net {
    struct endpoint;
}

namespace bee::net::socket {
#if defined _WIN32
    using fd_t = uintptr_t;
#else
    using fd_t = int;
#endif
    static inline const fd_t retired_fd = (fd_t)-1;

    enum class protocol {
        tcp = 0,
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

    enum class fdstat {
        success,
        wait,
        failed,
    };

    enum class status {
        success,
        wait,
        failed,
        close,
    };

    enum class option {
        reuseaddr = 0,
        sndbuf,
        rcvbuf,
    };

    bool   initialize();
    fd_t   open(protocol protocol);
    bool   pair(fd_t sv[2]);
#if !defined(_WIN32)
    bool   blockpair(fd_t sv[2]);
#endif
    bool   close(fd_t s);
    bool   shutdown(fd_t s, shutdown_flag flag);
    bool   setoption(fd_t s, option opt, int value);
    void   udp_connect_reset(fd_t s);
    bool   bind(fd_t s, const endpoint& ep);
    bool   listen(fd_t s, int backlog);
    fdstat connect(fd_t s, const endpoint& ep);
    fdstat accept(fd_t s, fd_t& newfd);
    fdstat accept(fd_t s, fd_t& newfd, endpoint& ep);
    status recv(fd_t s, int& rc, char* buf, int len);
    status send(fd_t s, int& rc, const char* buf, int len);
    status recvfrom(fd_t s, int& rc, char* buf, int len, endpoint& ep);
    status sendto(fd_t s, int& rc, const char* buf, int len, const endpoint& ep);
    bool   getpeername(fd_t s, endpoint& ep);
    bool   getsockname(fd_t s, endpoint& ep);
    bool   unlink(const endpoint& ep);
    int    errcode(fd_t s);
    fd_t   dup(fd_t s);
}
