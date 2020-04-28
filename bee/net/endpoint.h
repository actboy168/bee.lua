#pragma once

#include <string>
#include <string_view>
#include <bee/utility/dynarray.h>

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

    struct endpoint : private dynarray<std::byte> {
        static const size_t kMaxSize = 256;
        typedef dynarray<std::byte> mybase;
        endpoint_info   info() const;
        const sockaddr* addr() const;
        socklen_t       addrlen() const;
        sockaddr*       addr();
        void            resize(socklen_t len);
        int             family() const;
        bool            valid() const;

        static endpoint from_hostname(const std::string_view& ip, int port);
        static endpoint from_unixpath(const std::string_view& path);
        static endpoint from_empty();
        static endpoint from_invalid();

    private:
        endpoint(size_t n);
    };
}
