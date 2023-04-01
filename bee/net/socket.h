#pragma once

#if defined _WIN32
#    include <cstdint>
#endif

#include <bee/nonstd/expected.h>

#include <optional>
#include <system_error>

namespace bee::net {
    struct endpoint;
}

namespace bee::net::socket {
#if defined _WIN32
    using fd_t = uintptr_t;
#else
    using fd_t = int;
#endif
    static constexpr inline fd_t retired_fd = (fd_t)-1;

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

    bool initialize() noexcept;
    fd_t open(protocol protocol);
    bool pair(fd_t sv[2]);
#if !defined(_WIN32)
    bool blockpair(fd_t sv[2]) noexcept;
#endif
    bool close(fd_t s) noexcept;
    bool shutdown(fd_t s, shutdown_flag flag) noexcept;
    bool setoption(fd_t s, option opt, int value) noexcept;
    void udp_connect_reset(fd_t s) noexcept;
    bool bind(fd_t s, const endpoint& ep);
    bool listen(fd_t s, int backlog) noexcept;
    fdstat connect(fd_t s, const endpoint& ep);
    fdstat accept(fd_t s, fd_t& newfd) noexcept;
    status recv(fd_t s, int& rc, char* buf, int len) noexcept;
    status send(fd_t s, int& rc, const char* buf, int len) noexcept;
    expected<endpoint, status> recvfrom(fd_t s, int& rc, char* buf, int len);
    status sendto(fd_t s, int& rc, const char* buf, int len, const endpoint& ep) noexcept;
    std::optional<endpoint> getpeername(fd_t s);
    std::optional<endpoint> getsockname(fd_t s);
    bool unlink(const endpoint& ep);
    std::error_code errcode(fd_t s) noexcept;
    fd_t dup(fd_t s) noexcept;
}
