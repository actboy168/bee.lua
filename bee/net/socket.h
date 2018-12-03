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
    enum { retired_fd = -1 };

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
    int  connect(fd_t s, const endpoint& ep);
    int  bind(fd_t s, const endpoint& ep);
    int  listen(fd_t s, int backlog);
    int  accept(fd_t s, fd_t& sock);
    int  accept(fd_t s, fd_t& fd, endpoint& ep);
    int  recv(fd_t s, char* buf, int len);
    int  send(fd_t s, const char* buf, int len);
    int  recvfrom(fd_t s, char* buf, int len, endpoint& ep);
    int  sendto(fd_t s, const char* buf, int len, const endpoint& ep);
    bool getpeername(fd_t s, endpoint& ep);
    bool getsockname(fd_t s, endpoint& ep);
    bool unlink(fd_t s);
    int  errcode(fd_t s);
}
