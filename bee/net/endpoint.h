#pragma once

#include <bee/utility/dynarray.h>
#include <bee/utility/zstring_view.h>

#include <cstddef>
#include <string>

#if !defined(_WIN32)
#    include <sys/socket.h>
#endif

struct sockaddr;

namespace bee::net {
#if defined(_WIN32)
    using socklen_t = int;
#endif

    enum class un_format : uint16_t {
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
        sockaddr* addr() noexcept;
        socklen_t* addrlen() noexcept;

    private:
        friend struct endpoint;
        dynarray<std::byte> m_data;
        socklen_t m_size = 0;
    };

    struct endpoint {
        endpoint_info info() const;
        const sockaddr* addr() const noexcept;
        socklen_t addrlen() const noexcept;
        unsigned short family() const noexcept;
        bool valid() const noexcept;

        static endpoint from_hostname(zstring_view ip, uint16_t port);
        static endpoint from_unixpath(zstring_view path);
        static endpoint from_buf(endpoint_buf&& buf) noexcept;
        static endpoint from_invalid() noexcept;

    private:
        endpoint() noexcept;
        endpoint(size_t size);
        endpoint(std::byte const* data, size_t size);
        endpoint(dynarray<std::byte>&& data) noexcept;
        endpoint(dynarray<std::byte>&& data, size_t size) noexcept;
        dynarray<std::byte> m_data;
    };
}
