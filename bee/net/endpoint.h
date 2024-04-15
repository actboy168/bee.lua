#pragma once

#include <bee/utility/zstring_view.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <tuple>

struct sockaddr;

namespace bee::net {
#if defined(_WIN32)
    using socklen_t = int;
#else
    using socklen_t = unsigned int;
#endif

    constexpr socklen_t kMaxEndpointSize = 256;

    enum class un_format : uint8_t {
        pathname,
        abstract,
        unnamed,
        invalid,
    };

    enum class family : uint8_t {
        unknown,
        unix,
        inet,
        inet6,
    };

    struct endpoint {
        endpoint() noexcept;
        uint16_t port() const noexcept;
        std::tuple<std::string, uint16_t> get_inet() const noexcept;
        std::tuple<std::string, uint16_t> get_inet6() const noexcept;
        std::tuple<un_format, zstring_view> get_unix() const noexcept;
        family get_family() const noexcept;
        const sockaddr* addr() const noexcept;
        socklen_t addrlen() const noexcept;
        bool valid() const noexcept;
        sockaddr* out_addr() noexcept;
        socklen_t* out_addrlen() noexcept;

        static endpoint from_hostname(zstring_view name, uint16_t port) noexcept;
        static endpoint from_ip(zstring_view ip, uint16_t port) noexcept;
        static endpoint from_unixpath(zstring_view path) noexcept;

    private:
        endpoint(const std::byte* data, size_t size) noexcept;

    private:
        std::byte m_data[kMaxEndpointSize];
        socklen_t m_size;
    };
    static_assert(std::is_trivially_destructible_v<endpoint>);
}
