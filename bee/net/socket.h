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
    inline static const fd_t retired_fd = -1;

    enum class protocol {
        none,
        tcp,
        udp,
        unix,
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
 
    void initialize();
    fd_t open(int family, protocol protocol);
    bool close(fd_t s);
    bool shutdown(fd_t s, shutdown_flag flag);
    void nonblocking(fd_t s);
    void keepalive(fd_t s, int keepalive, int keepalive_cnt, int keepalive_idle, int keepalive_intvl);
    void udp_connect_reset(fd_t s);
    void send_buffer(fd_t s, int bufsize);
    void recv_buffer(fd_t s, int bufsize);
    void reuse(fd_t s);
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
    int    errcode(fd_t s);
    bool   supportUnixDomainSocket();
}
