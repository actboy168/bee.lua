#pragma once

#include <string>
#if defined _WIN32
#include <stdint.h>
#endif

namespace bee::net {
    struct endpoint;
}

namespace bee::net::socket {

#if defined _WIN32
    typedef uintptr_t fd_t;
#else
    typedef int fd_t;
#endif
    inline static const fd_t retired_fd = (fd_t)-1;

    enum class protocol {
        tcp = 0,
        udp,
        uds,
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
        close,
        failed,
    };

    enum class option {
        reuseaddr = 0,
        sndbuf,
        rcvbuf,
    };

    void initialize();
    fd_t open(protocol protocol);
    bool pair(fd_t sv[2]);
#if !defined(_WIN32)
    bool blockpair(fd_t sv[2]);
#endif
    bool close(fd_t s);
    bool shutdown(fd_t s, shutdown_flag flag);
    void setoption(fd_t s, option opt, int value);
    void keepalive(fd_t s, int keepalive, int keepalive_cnt, int keepalive_idle, int keepalive_intvl);
    void udp_connect_reset(fd_t s);
    status connect(fd_t s, const endpoint& ep);
    status bind(fd_t s, const endpoint& ep);
    status listen(fd_t s, int backlog);
    status accept(fd_t s, fd_t& sock);
    status accept(fd_t s, fd_t& fd, endpoint& ep);
    status recv(fd_t s, int& rc, char* buf, int len);
    status send(fd_t s, int& rc, const char* buf, int len);
    status recvfrom(fd_t s, int& rc, char* buf, int len, endpoint& ep);
    status sendto(fd_t s, int& rc, const char* buf, int len, const endpoint& ep);
    bool   getpeername(fd_t s, endpoint& ep);
    bool   getsockname(fd_t s, endpoint& ep);
    bool   unlink(fd_t s);
    bool   unlink(const endpoint& ep);
    int    errcode(fd_t s);
    fd_t   dup(fd_t s);
#if defined _WIN32
    bool   supportUnixDomainSocket();
    void   simulationUnixDomainSocket(bool open);
#endif
}
