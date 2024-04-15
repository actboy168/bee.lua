#include <bee/net/endpoint.h>
#if defined(_WIN32)
#    include <Ws2tcpip.h>
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
#include <bee/error.h>
#include <bee/nonstd/charconv.h>

#include <array>
#include <limits>

// see the https://blogs.msdn.microsoft.com/commandline/2017/12/19/af_unix-comes-to-windows/
#if defined(_WIN32)
//
// need Windows SDK >= 17063
// #include <afunix.h>
//
#    define UNIX_PATH_MAX 108
struct sockaddr_un {
    unsigned short sun_family;
    char sun_path[UNIX_PATH_MAX];
};
#endif

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
    }

    std::optional<endpoint> endpoint::from_unixpath(zstring_view path) noexcept {
        if (path.size() >= UNIX_PATH_MAX) {
            return std::nullopt;
        }
        struct sockaddr_un su;
        su.sun_family = AF_UNIX;
        memset(su.sun_path, 0, UNIX_PATH_MAX);
        memcpy(su.sun_path, path.data(), path.size() + 1);
        return endpoint { su };
    }

    endpoint endpoint::from_localhost(uint16_t port) noexcept {
        struct sockaddr_in sa4;
        auto r = inet_pton(AF_INET, "127.0.0.1", &sa4.sin_addr);
        assert(r == 1);
        sa4.sin_family = AF_INET;
        sa4.sin_port   = htons(port);
        return { sa4 };
    }

    std::optional<endpoint> endpoint::from_hostname(zstring_view name, uint16_t port) noexcept {
        constexpr auto portn = std::numeric_limits<uint16_t>::digits10 + 1;
        char portstr[portn + 1];
        if (auto [p, ec] = std::to_chars(portstr, portstr + portn, port); ec != std::errc()) {
            return std::nullopt;
        }
        else {
            p[0] = '\0';
        }
        AddrInfo info(name, portstr);
        if (!info) {
            return std::nullopt;
        }
        if (info->ai_addrlen == 0 || info->ai_addrlen > kMaxEndpointSize) {
            return std::nullopt;
        }
        if (info->ai_family == AF_INET) {
            if (info->ai_addrlen != sizeof(sockaddr_in)) {
                return std::nullopt;
            }
            return endpoint { *(const sockaddr_in*)info->ai_addr };
        }
        else if (info->ai_family == AF_INET6) {
            if (info->ai_addrlen != sizeof(sockaddr_in6)) {
                return std::nullopt;
            }
            return endpoint { *(const sockaddr_in6*)info->ai_addr };
        }
        else {
            return std::nullopt;
        }
    }

    endpoint::endpoint() noexcept
        : m_data()
        , m_size(0) {}

    std::tuple<std::string, uint16_t> endpoint::get_inet() const noexcept {
        const sockaddr* sa = addr();
        char tmp[sizeof "255.255.255.255"];
#if !defined(__MINGW32__)
        const char* s = inet_ntop(AF_INET, (const void*)&((struct sockaddr_in*)sa)->sin_addr, tmp, sizeof tmp);
#else
        const char* s = inet_ntop(AF_INET, (void*)&((struct sockaddr_in*)sa)->sin_addr, tmp, sizeof tmp);
#endif
        return { std::string(s), ntohs(((struct sockaddr_in*)sa)->sin_port) };
    }

    std::tuple<std::string, uint16_t> endpoint::get_inet6() const noexcept {
        const sockaddr* sa = addr();
        char tmp[sizeof "ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255"];
#if !defined(__MINGW32__)
        const char* s = inet_ntop(AF_INET6, (const void*)&((const struct sockaddr_in6*)sa)->sin6_addr, tmp, sizeof tmp);
#else
        const char* s = inet_ntop(AF_INET6, (void*)&((const struct sockaddr_in6*)sa)->sin6_addr, tmp, sizeof tmp);
#endif
        return { std::string(s), ntohs(((struct sockaddr_in6*)sa)->sin6_port) };
    }

    std::tuple<un_format, zstring_view> endpoint::get_unix() const noexcept {
        const sockaddr* sa = addr();
        if (sa->sa_family != AF_UNIX) {
            assert(false);
            return { un_format::invalid, {} };
        }
        const char* path = ((struct sockaddr_un*)sa)->sun_path;
        const size_t len = m_size - offsetof(struct sockaddr_un, sun_path) - 1;
        if (len > 0 && path[0] != 0) {
            return { un_format::pathname, { path, len } };
        }
        else if (len > 1) {
            return { un_format::abstract, { path + 1, len - 1 } };
        }
        else {
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
            assert(false);
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
}
