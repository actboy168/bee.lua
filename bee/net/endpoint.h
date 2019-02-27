#pragma once

#include <string>
#include <string_view>
#include <bee/nonstd/dynarray.h>
#include <bee/nonstd/expected.h>

#if !defined(_WIN32)
#include <sys/socket.h>
#endif

struct sockaddr;

namespace bee::net {
#if defined(_WIN32)
    typedef int socklen_t;
#endif

    struct endpoint_info {
        std::string ip;
        int port;
    };

    struct endpoint : private std::dynarray<std::byte> {
        static const size_t kMaxSize = 256;
        typedef std::dynarray<std::byte> mybase;
        endpoint_info   info() const;
        const sockaddr* addr() const;
        socklen_t       addrlen() const;
        sockaddr*       addr();
        void            resize(socklen_t len);
        int             family() const;

        static nonstd::expected<endpoint, std::string> from_hostname(const std::string_view& ip, int port);
        static nonstd::expected<endpoint, std::string> from_unixpath(const std::string_view& path);
        static endpoint                                from_empty();

    private:
        endpoint(size_t n);
    };
}
