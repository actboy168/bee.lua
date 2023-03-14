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

    struct endpoint_buf {
        endpoint_buf();
        endpoint_buf(size_t size);
        sockaddr*  addr();
        socklen_t* addrlen();
    private:
        friend struct endpoint;
        dynarray<std::byte> m_data;
        socklen_t m_size = 0;
    };

    struct endpoint {
        endpoint_info   info() const;
        const sockaddr* addr() const;
        socklen_t       addrlen() const;
        int             family() const;
        bool            valid() const;

        static endpoint from_hostname(zstring_view ip, uint16_t port);
        static endpoint from_unixpath(zstring_view path);
        static endpoint from_buf(endpoint_buf&& buf);
        static endpoint from_invalid();

    private:
        endpoint();
        endpoint(size_t size);
        endpoint(std::byte const* data, size_t size);
        endpoint(dynarray<std::byte>&& data);
        endpoint(dynarray<std::byte>&& data, size_t size);
        dynarray<std::byte> m_data;
    };
}
