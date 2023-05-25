#pragma once

#include <bee/utility/zstring_view.h>

#include <cstddef>
#include <cstdint>
#include <string>

struct sockaddr;

namespace bee::net {
#if defined(_WIN32)
    using socklen_t = int;
#else
    using socklen_t = unsigned int;
#endif

    constexpr socklen_t kMaxEndpointSize = 256;

    enum class un_format : uint16_t {
        pathname = 0,
        abstract,
        unnamed,
    };

    struct endpoint_info {
        std::string ip;
        uint16_t port;
    };

    struct endpoint {
        endpoint() noexcept;
        endpoint_info info() const;
        const sockaddr* addr() const noexcept;
        socklen_t addrlen() const noexcept;
        unsigned short family() const noexcept;
        bool valid() const noexcept;
        sockaddr* out_addr() noexcept;
        socklen_t* out_addrlen() noexcept;

        static endpoint from_hostname(zstring_view ip, uint16_t port) noexcept;
        static endpoint from_unixpath(zstring_view path) noexcept;

    private:
        endpoint(const std::byte* data, size_t size) noexcept;

    private:
        std::byte m_data[kMaxEndpointSize];
        socklen_t m_size;
    };
    static_assert(std::is_trivially_destructible_v<endpoint>);
}
