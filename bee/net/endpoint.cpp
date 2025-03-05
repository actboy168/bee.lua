#include <bee/net/endpoint.h>
#if defined(_WIN32)
#    include <Ws2tcpip.h>
// see the https://blogs.msdn.microsoft.com/commandline/2017/12/19/af_unix-comes-to-windows/
//
// need Windows SDK >= 17063
// #include <afunix.h>
//
#    define UNIX_PATH_MAX 108
struct sockaddr_un {
    unsigned short sun_family;
    char sun_path[UNIX_PATH_MAX];
};
#else
#    include <arpa/inet.h>
#    include <netdb.h>
#    include <sys/un.h>
#    if defined(__FreeBSD__) || defined(__OpenBSD__)
#        include <netinet/in.h>
#        include <sys/socket.h>
#    endif
#    ifndef UNIX_PATH_MAX
#        define UNIX_PATH_MAX (sizeof(sockaddr_un::sun_path) / sizeof(sockaddr_un::sun_path[0]))
#    endif
#endif
#include <bee/net/ip.h>
#include <bee/nonstd/charconv.h>

#include <array>
#include <limits>

namespace bee::net {
    static_assert(sizeof(sockaddr_in) <= kMaxEndpointSize);
    static_assert(sizeof(sockaddr_in6) <= kMaxEndpointSize);
    static_assert(sizeof(sockaddr_un) <= kMaxEndpointSize);

    namespace {
        struct AddrInfo {
            AddrInfo(zstring_view name, const char* port) noexcept {
                addrinfo hint  = {};
                hint.ai_family = AF_UNSPEC;
                if (needsnolookup(name)) {
                    hint.ai_flags = AI_NUMERICHOST;
                }
                const int err = ::getaddrinfo(name.data(), port, &hint, &info);
                if (err != 0) {
                    if (info) {
                        ::freeaddrinfo(info);
                        info = nullptr;
                    }
                }
            }
            ~AddrInfo() noexcept {
                if (info) {
                    ::freeaddrinfo(info);
                }
            }
            explicit operator bool() const noexcept {
                return !!info;
            }
            const addrinfo* operator->() const noexcept {
                return info;
            }
            static bool needsnolookup(zstring_view name) noexcept {
                size_t pos = name.find_first_not_of("0123456789.");
                if (pos == std::string_view::npos) {
                    return true;
                }
                pos = name.find_first_not_of("0123456789abcdefABCDEF:");
                if (pos == std::string_view::npos) {
                    return true;
                }
                if (name[pos] != '.') {
                    return false;
                }
                pos = name.find_last_of(':');
                if (pos == std::string_view::npos) {
                    return false;
                }
                pos = name.find_first_not_of("0123456789.", pos);
                if (pos == std::string_view::npos) {
                    return true;
                }
                return false;
            }
            addrinfo* info = nullptr;
        };

        const char* inet_ntop_wrapper(int af, const void* src, char* dst, socklen_t size) {
#if !defined(__MINGW32__)
            return inet_ntop(af, src, dst, size);
#else
            return inet_ntop(af, const_cast<void*>(src), dst, size);
#endif
        }
    }

    bool endpoint::ctor_hostname(endpoint& ep, zstring_view name, uint16_t port) noexcept {
        constexpr auto portn = std::numeric_limits<uint16_t>::digits10 + 1;
        char portstr[portn + 1];
        if (auto [p, ec] = std::to_chars(portstr, portstr + portn, port); ec != std::errc()) {
            return false;
        } else {
            p[0] = '\0';
        }
        AddrInfo info(name, portstr);
        if (!info) {
            return false;
        }
        if (info->ai_addrlen == 0 || info->ai_addrlen > kMaxEndpointSize) {
            return false;
        }
        if (info->ai_family == AF_INET) {
            if (info->ai_addrlen != sizeof(sockaddr_in)) {
                return false;
            }
            ep.assign(*(const sockaddr_in*)info->ai_addr);
            return true;
        } else if (info->ai_family == AF_INET6) {
            if (info->ai_addrlen != sizeof(sockaddr_in6)) {
                return false;
            }
            ep.assign(*(const sockaddr_in6*)info->ai_addr);
            return true;
        } else {
            return false;
        }
    }

    bool endpoint::ctor_unix(endpoint& ep, zstring_view path) noexcept {
        if (path.size() >= UNIX_PATH_MAX) {
            return false;
        }
        struct sockaddr_un su;
        su.sun_family = AF_UNIX;
        memset(su.sun_path, 0, UNIX_PATH_MAX);
        memcpy(su.sun_path, path.data(), path.size() + 1);
        ep.assign(su);
        return true;
    }

    bool endpoint::ctor_inet(endpoint& ep, zstring_view ip, uint16_t port) noexcept {
        struct sockaddr_in sa4;
        if (1 == inet_pton(AF_INET, ip.data(), &sa4.sin_addr)) {
            sa4.sin_family = AF_INET;
            sa4.sin_port   = htons(port);
            ep.assign(sa4);
            return true;
        }
        return false;
    }

    bool endpoint::ctor_inet6(endpoint& ep, zstring_view ip, uint16_t port) noexcept {
        struct sockaddr_in6 sa6;
        if (1 == inet_pton(AF_INET6, ip.data(), &sa6.sin6_addr)) {
            sa6.sin6_family = AF_INET6;
            sa6.sin6_port   = htons(port);
            ep.assign(sa6);
            return true;
        }
        return false;
    }

    endpoint endpoint::from_localhost(uint16_t port) noexcept {
        struct sockaddr_in sa4;
        sa4.sin_family = AF_INET;
        sa4.sin_port   = htons(port);
        sa4.sin_addr   = std::bit_cast<decltype(sa4.sin_addr)>(ip::inet_pton_v4("127.0.0.1"));
        endpoint ep;
        ep.assign(sa4);
        return ep;
    }

    endpoint::endpoint() noexcept
        : m_data()
        , m_size(0) {}

    std::tuple<std::string, uint16_t> endpoint::get_inet() const noexcept {
        const sockaddr* sa = addr();
        char tmp[sizeof "255.255.255.255"];
        const char* s = inet_ntop_wrapper(AF_INET, &((struct sockaddr_in*)sa)->sin_addr, tmp, sizeof tmp);
        return { std::string(s), ntohs(((struct sockaddr_in*)sa)->sin_port) };
    }

    std::tuple<std::string, uint16_t> endpoint::get_inet6() const noexcept {
        const sockaddr* sa = addr();
        char tmp[sizeof "ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255"];
        const char* s = inet_ntop_wrapper(AF_INET6, &((const struct sockaddr_in6*)sa)->sin6_addr, tmp, sizeof tmp);
        return { std::string(s), ntohs(((struct sockaddr_in6*)sa)->sin6_port) };
    }

    std::tuple<un_format, zstring_view> endpoint::get_unix() const noexcept {
        const sockaddr* sa = addr();
        if (sa->sa_family != AF_UNIX) {
            return { un_format::invalid, {} };
        }
        const char* path = ((struct sockaddr_un*)sa)->sun_path;
        const size_t len = m_size - offsetof(struct sockaddr_un, sun_path) - 1;
        if (len > 0 && path[0] != 0) {
            return { un_format::pathname, { path, len } };
        } else if (len > 1) {
            return { un_format::abstract, { path + 1, len - 1 } };
        } else {
            return { un_format::unnamed, {} };
        }
    }

    family endpoint::get_family() const noexcept {
        const sockaddr* sa = addr();
        switch (sa->sa_family) {
        case AF_INET:
            return family::inet;
        case AF_INET6:
            return family::inet6;
        case AF_UNIX:
            return family::unix;
        default:
            return family::unknown;
        }
    }

    uint16_t endpoint::get_port() const noexcept {
        const sockaddr* sa = addr();
        switch (sa->sa_family) {
        case AF_INET:
            return ntohs(((struct sockaddr_in*)sa)->sin_port);
        case AF_INET6:
            return ntohs(((struct sockaddr_in6*)sa)->sin6_port);
        default:
            return 0;
        }
    }

    const sockaddr* endpoint::addr() const noexcept {
        return (const sockaddr*)m_data;
    }

    socklen_t endpoint::addrlen() const noexcept {
        return (socklen_t)m_size;
    }

    sockaddr* endpoint::out_addr() noexcept {
        return (sockaddr*)m_data;
    }

    socklen_t* endpoint::out_addrlen() noexcept {
        m_size = kMaxEndpointSize;
        return &m_size;
    }

    bool endpoint::operator==(const endpoint& o) const noexcept {
        if (get_family() != o.get_family())
            return false;
        const sockaddr* saddr = addr();
        switch (saddr->sa_family) {
        case AF_INET: {
            const sockaddr_in* sa = (struct sockaddr_in*)saddr;
            const sockaddr_in* sb = (struct sockaddr_in*)o.addr();
            if (sa->sin_port != sb->sin_port)
                return false;
            return 0 == memcmp(&sa->sin_addr, &sb->sin_addr, sizeof(sa->sin_addr));
        }
        case AF_INET6: {
            const sockaddr_in6* sa = (struct sockaddr_in6*)saddr;
            const sockaddr_in6* sb = (struct sockaddr_in6*)o.addr();
            if (sa->sin6_port != sb->sin6_port)
                return false;
            return 0 == memcmp(&sa->sin6_addr, &sb->sin6_addr, sizeof(sa->sin6_addr));
        }
        case AF_UNIX: {
            auto [type_a, path_a] = get_unix();
            auto [type_b, path_b] = o.get_unix();
            return (type_a == type_b) && (path_a == path_b);
        }
        default:
            return false;
        }
    }
}
