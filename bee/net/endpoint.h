#pragma once

#include <bee/utility/zstring_view.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <tuple>
#include <type_traits>

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
        std::tuple<std::string, uint16_t> get_inet() const noexcept;
        std::tuple<std::string, uint16_t> get_inet6() const noexcept;
        std::tuple<un_format, zstring_view> get_unix() const noexcept;
        family get_family() const noexcept;
        uint16_t get_port() const noexcept;
        const sockaddr* addr() const noexcept;
        socklen_t addrlen() const noexcept;
        sockaddr* out_addr() noexcept;
        socklen_t* out_addrlen() noexcept;
        bool operator==(const endpoint& o) const noexcept;
        template <typename SOCKADDR>
        void assign(const SOCKADDR& v) noexcept {
            m_size = (socklen_t)sizeof(v);
            memcpy(m_data, &v, sizeof(v));
        }

        static bool ctor_hostname(endpoint& ep, zstring_view name, uint16_t port) noexcept;
        static bool ctor_unix(endpoint& ep, zstring_view path) noexcept;
        static bool ctor_inet(endpoint& ep, zstring_view ip, uint16_t port) noexcept;
        static bool ctor_inet6(endpoint& ep, zstring_view ip, uint16_t port) noexcept;
        static endpoint from_localhost(uint16_t port) noexcept;

    private:
        std::byte m_data[kMaxEndpointSize];
        socklen_t m_size;
    };
    static_assert(std::is_trivially_destructible_v<endpoint>);
}
