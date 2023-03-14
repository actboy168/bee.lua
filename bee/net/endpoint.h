#pragma once

#include <cstddef>
#include <string>
#include <bee/utility/dynarray.h>
#include <bee/utility/zstring_view.h>

#if !defined(_WIN32)
#include <sys/socket.h>
#endif

struct sockaddr;

namespace bee::net {
#if defined(_WIN32)
    using socklen_t = int;
#endif

    enum class un_format: uint16_t {
        pathname = 0,
        abstract,
        unnamed,
    };

    struct endpoint_info {
        std::string ip;
        uint16_t port;
    };

    struct endpoint {
        endpoint_info   info() const;
        const sockaddr* addr() const;
        socklen_t       addrlen() const;
        sockaddr*       addr();
        void            resize(socklen_t len);
        int             family() const;
        bool            valid() const;

        static endpoint from_hostname(zstring_view ip, uint16_t port);
        static endpoint from_unixpath(zstring_view path);
        static endpoint from_empty();
        static endpoint from_invalid();

    private:
        endpoint(size_t n);
        dynarray<std::byte> m_data;
        size_t m_size = 0;
    };
}
