#if defined _WIN32
//  clang-format off
#    include <winsock2.h>
//  clang-format on
#    include <bee/nonstd/charconv.h>
#    include <bee/platform/win/unicode.h>
#    include <bee/platform/win/unlink.h>
#    include <bee/utility/dynarray.h>
#    include <mstcpip.h>
#    include <mswsock.h>

#    include <array>
#    include <fstream>
#    include <limits>
#else
#    include <fcntl.h>
#    include <netinet/in.h>
#    include <netinet/tcp.h>
#    include <signal.h>
#    include <unistd.h>
#    if defined(__APPLE__)
#        include <sys/ioctl.h>
#    endif
#endif

#include <bee/error.h>
#include <bee/net/endpoint.h>
#include <bee/net/socket.h>
#include <bee/nonstd/unreachable.h>

#include <cassert>

#define net_success(x) ((x) == 0)

#if defined(__MINGW32__)
#    define WSA_FLAG_NO_HANDLE_INHERIT 0x80
#endif

namespace bee::net::socket {
#if defined(_WIN32)
    static_assert(sizeof(SOCKET) == sizeof(fd_t));

#    if defined(_MSC_VER)
#        define FILENAME(n) win::u2w(n)
#    else
#        define FILENAME(n) (n)
#    endif

    static std::string file_read(const std::string& filename) {
        std::fstream f(FILENAME(filename), std::ios::binary | std::ios::in);
        if (!f) return std::string();
        return std::string((std::istreambuf_iterator<char>(f)), (std::istreambuf_iterator<char>()));
    }
    static bool file_write(const std::string& filename, const std::string& value) {
        std::fstream f(FILENAME(filename), std::ios::binary | std::ios::out);
        if (!f) return false;
        std::copy(value.begin(), value.end(), std::ostreambuf_iterator<char>(f));
        return true;
    }

    static bool read_tcp_port(const endpoint& ep, uint16_t& tcpport) {
        auto [path, type] = ep.info();
        if (type != (uint16_t)un_format::pathname) {
            return false;
        }
        auto unixpath = file_read(path);
        if (unixpath.empty()) {
            return false;
        }
        if (auto [p, ec] = std::from_chars(unixpath.data(), unixpath.data() + unixpath.size(), tcpport); ec != std::errc()) {
            return false;
        }
        if (tcpport <= 0 || tcpport > (std::numeric_limits<uint16_t>::max)()) {
            return false;
        }
        return true;
    }

    static bool write_tcp_port(const std::string& path, fd_t s) {
        if (auto ep_opt = socket::getsockname(s)) {
            auto [ip, tcpport] = ep_opt->info();
            std::array<char, 10> portstr;
            if (auto [p, ec] = std::to_chars(portstr.data(), portstr.data() + portstr.size() - 1, tcpport); ec != std::errc()) {
                return false;
            }
            else {
                p[0] = '\0';
            }
            return file_write(path, portstr.data());
        }
        return false;
    }

    static fdstat u_connect(fd_t s, const endpoint& ep) {
        uint16_t tcpport = 0;
        if (!read_tcp_port(ep, tcpport)) {
            ::WSASetLastError(WSAECONNREFUSED);
            return fdstat::failed;
        }
        auto newep = endpoint::from_hostname("127.0.0.1", tcpport);
        if (!newep.valid()) {
            ::WSASetLastError(WSAECONNREFUSED);
            return fdstat::failed;
        }
        return socket::connect(s, newep);
    }

    static bool u_bind(fd_t s, const endpoint& ep) {
        auto newep = endpoint::from_hostname("127.0.0.1", 0);
        if (!newep.valid()) {
            return false;
        }
        const bool ok = socket::bind(s, newep);
        if (!ok) {
            return ok;
        }
        auto [path, type] = ep.info();
        if (type != (uint16_t)un_format::pathname) {
            ::WSASetLastError(WSAENETDOWN);
            return false;
        }
        if (!write_tcp_port(path, s)) {
            ::WSASetLastError(WSAENETDOWN);
            return false;
        }
        return true;
    }

    static WSAPROTOCOL_INFOW UnixProtocol;
    static bool supportUnixDomainSocket_() {
        static GUID AF_UNIX_PROVIDER_ID = { 0xA00943D9, 0x9C2E, 0x4633, { 0x9B, 0x59, 0x00, 0x57, 0xA3, 0x16, 0x09, 0x94 } };
        DWORD len                       = 0;
        ::WSAEnumProtocolsW(0, NULL, &len);
        dynarray<std::byte> buf(len);
        LPWSAPROTOCOL_INFOW protocols = (LPWSAPROTOCOL_INFOW)buf.data();
        const int n                   = ::WSAEnumProtocolsW(0, protocols, &len);
        if (n == SOCKET_ERROR) {
            return false;
        }
        for (int i = 0; i < n; ++i) {
            if (protocols[i].iAddressFamily == AF_UNIX && IsEqualGUID(protocols[i].ProviderId, AF_UNIX_PROVIDER_ID)) {
                const fd_t fd = ::WSASocketW(PF_UNIX, SOCK_STREAM, 0, &protocols[i], 0, WSA_FLAG_NO_HANDLE_INHERIT);
                if (fd == retired_fd) {
                    return false;
                }
                ::closesocket(fd);
                UnixProtocol = protocols[i];
                return true;
            }
        }
        return false;
    }
    static bool supportUnixDomainSocket() {
        static bool support = supportUnixDomainSocket_();
        return support;
    }
#endif

    bool initialize() noexcept {
        static bool initialized = false;
        if (!initialized) {
#if defined(_WIN32)
            WSADATA wd;
            const int rc = ::WSAStartup(MAKEWORD(2, 2), &wd);
            ::SetLastError(rc);
            initialized = rc == 0;
#else
            initialized = true;
#    if !defined(MSG_NOSIGNA) && !defined(SO_NOSIGPIPE)
            struct sigaction sa;
            sa.sa_handler = SIG_IGN;
            sa.sa_flags   = 0;
            sigemptyset(&sa.sa_mask);
            sigaction(SIGPIPE, &sa, 0);
#    endif
#endif
        }
        return initialized;
    }

    static bool wait_finish() noexcept {
#if defined(_WIN32)
        switch (::WSAGetLastError()) {
        case WSAEINPROGRESS:
        case WSAEWOULDBLOCK:
            return true;
        default:
            return false;
        }
#else
        switch (errno) {
        case EAGAIN:
        case EINTR:
#    if EAGAIN != EWOULDBLOCK
        case EWOULDBLOCK:
#    endif
            return true;
        default:
            return false;
        }
#endif
    }

#if defined(_WIN32)
    static bool set_nonblock(fd_t s, bool set) noexcept {
        unsigned long nonblock = set ? 1 : 0;
        const int ok           = ::ioctlsocket(s, FIONBIO, &nonblock);
        return net_success(ok);
    }
    static bool set_cloexec(fd_t s, bool set) noexcept {
        if (set) {
            DWORD flags = 0;
            (void)flags;
            assert(::GetHandleInformation((HANDLE)s, &flags) && (flags & HANDLE_FLAG_INHERIT) == 0);
            return true;
        }
        else {
            return !!SetHandleInformation((HANDLE)s, HANDLE_FLAG_INHERIT, 0);
        }
    }
#elif defined(__APPLE__)
    static bool set_nonblock(int fd, bool set) noexcept {
        int r;
        do
            r = ::fcntl(fd, F_GETFL);
        while (r == -1 && errno == EINTR);
        if (r == -1)
            return false;
        if (!!(r & O_NONBLOCK) == set)
            return true;
        int flags = set ? (r | O_NONBLOCK) : (r & ~O_NONBLOCK);
        do
            r = ::fcntl(fd, F_SETFL, flags);
        while (!net_success(r) && errno == EINTR);
        return net_success(r);
    }
    static bool set_cloexec(fd_t fd, bool set) noexcept {
        int ok;
        do
            ok = ::ioctl(fd, set ? FIOCLEX : FIONCLEX);
        while (!net_success(ok) && errno == EINTR);
        return net_success(ok);
    }
#endif

    bool close(fd_t s) noexcept {
#if defined _WIN32
        const int ok = ::closesocket(s);
#else
        const int ok = ::close(s);
#endif
        return net_success(ok);
    }

    static int get_error() noexcept {
#if defined(_WIN32)
        return ::WSAGetLastError();
#else
        return errno;
#endif
    }

#if defined(_WIN32) || defined(__APPLE__)
    static void set_error(int err) noexcept {
#    if defined(_WIN32)
        ::WSASetLastError(err);
#    else
        errno = err;
#    endif
    }

    static void internal_close(fd_t s) noexcept {
        const int saved_errno = get_error();
        close(s);
        set_error(saved_errno);
    }
#endif

    template <typename T>
    static bool setoption(fd_t s, int level, int optname, T& v) noexcept {
        const int ok = ::setsockopt(s, level, optname, (char*)&v, sizeof(T));
        return net_success(ok);
    }

    static fd_t createSocket(int af, int type, int protocol, fd_flags fd_flags) noexcept {
#if defined(_WIN32)
        const fd_t fd = ::WSASocketW(af, type, protocol, af == PF_UNIX ? &UnixProtocol : NULL, 0, WSA_FLAG_NO_HANDLE_INHERIT);
        if (fd == retired_fd) {
            return retired_fd;
        }
        if (!set_nonblock(fd, fd_flags == fd_flags::nonblock)) {
            ::closesocket(fd);
            return retired_fd;
        }
#elif defined(__APPLE__)
        const fd_t fd = ::socket(af, type, protocol);
        if (fd == retired_fd) {
            return retired_fd;
        }
        if (!set_cloexec(fd, true)) {
            internal_close(fd);
            return retired_fd;
        }
        if (!set_nonblock(fd, fd_flags == fd_flags::nonblock)) {
            internal_close(fd);
            return retired_fd;
        }
#else
        int flags = type | SOCK_CLOEXEC;
        if (fd_flags == fd_flags::nonblock) {
            flags |= SOCK_NONBLOCK;
        }
        const fd_t fd = ::socket(af, flags, protocol);
        if (fd == retired_fd) {
            return retired_fd;
        }
#endif

#ifdef SO_NOSIGPIPE
        const int enable = 1;
        if (!setoption(fd, SOL_SOCKET, SO_NOSIGPIPE, enable)) {
            internal_close(fd);
            return retired_fd;
        }
#endif
        return fd;
    }

    fd_t open(protocol protocol, fd_flags flags) {
        switch (protocol) {
        case protocol::tcp:
            return createSocket(PF_INET, SOCK_STREAM, IPPROTO_TCP, flags);
        case protocol::udp:
            return createSocket(PF_INET, SOCK_DGRAM, IPPROTO_UDP, flags);
        case protocol::tcp6:
            return createSocket(PF_INET6, SOCK_STREAM, IPPROTO_TCP, flags);
        case protocol::udp6:
            return createSocket(PF_INET6, SOCK_DGRAM, IPPROTO_UDP, flags);
        case protocol::unix:
#if defined _WIN32
            if (!supportUnixDomainSocket()) {
                return createSocket(PF_INET, SOCK_STREAM, IPPROTO_TCP, flags);
            }
#endif
            return createSocket(PF_UNIX, SOCK_STREAM, 0, flags);
        default:
            std::unreachable();
        }
    }

    bool shutdown(fd_t s, shutdown_flag flag) noexcept {
#if defined(_WIN32)
        switch (flag) {
        case shutdown_flag::both:
            return net_success(::shutdown(s, SD_BOTH));
        case shutdown_flag::read:
            return net_success(::shutdown(s, SD_RECEIVE));
        case shutdown_flag::write:
            return net_success(::shutdown(s, SD_SEND));
        default:
            std::unreachable();
        }
#else
        switch (flag) {
        case shutdown_flag::both:
            return net_success(::shutdown(s, SHUT_RDWR));
        case shutdown_flag::read:
            return net_success(::shutdown(s, SHUT_RD));
        case shutdown_flag::write:
            return net_success(::shutdown(s, SHUT_WR));
        default:
            std::unreachable();
        }
#endif
    }

    bool setoption(fd_t s, option opt, int value) noexcept {
        switch (opt) {
        case option::reuseaddr:
            return setoption(s, SOL_SOCKET, SO_REUSEADDR, value);
        case option::sndbuf:
            return setoption(s, SOL_SOCKET, SO_SNDBUF, value);
        case option::rcvbuf:
            return setoption(s, SOL_SOCKET, SO_RCVBUF, value);
        default:
            std::unreachable();
        }
    }

    void udp_connect_reset(fd_t s) noexcept {
#if defined _WIN32
        DWORD byte_retruned = 0;
        bool new_be         = false;
        ::WSAIoctl(s, SIO_UDP_CONNRESET, &new_be, sizeof(new_be), NULL, 0, &byte_retruned, NULL, NULL);
#else
        (void)s;
#endif
    }

    bool bind(fd_t s, const endpoint& ep) {
#if defined _WIN32
        if (!supportUnixDomainSocket() && ep.family() == AF_UNIX) {
            return u_bind(s, ep);
        }
#endif
        const int ok = ::bind(s, ep.addr(), ep.addrlen());
        return net_success(ok);
    }

    bool listen(fd_t s, int backlog) noexcept {
        const int ok = ::listen(s, backlog);
        return net_success(ok);
    }

    fdstat connect(fd_t s, const endpoint& ep) {
#if defined _WIN32
        if (!supportUnixDomainSocket() && ep.family() == AF_UNIX) {
            return u_connect(s, ep);
        }
#endif
        const int ok = ::connect(s, ep.addr(), ep.addrlen());
        if (net_success(ok)) {
            return fdstat::success;
        }

#if defined _WIN32
        const int error_code = ::WSAGetLastError();
        if (error_code == WSAEINPROGRESS || error_code == WSAEWOULDBLOCK)
            return fdstat::wait;
#else
        if (errno == EINTR || errno == EINPROGRESS)
            return fdstat::wait;
#endif
        return fdstat::failed;
    }

    static fd_t acceptEx(fd_t s, fd_flags fd_flags, struct sockaddr* addr, socklen_t* addrlen) noexcept {
#if defined(_WIN32) || defined(__APPLE__)
        const fd_t fd = ::accept(s, addr, addrlen);
        if (fd != retired_fd) {
            if (!set_cloexec(fd, true)) {
                internal_close(fd);
                return retired_fd;
            }
            if (!set_nonblock(fd, fd_flags == fd_flags::nonblock)) {
                internal_close(fd);
                return retired_fd;
            }
        }
        return fd;
#else
        int flags = SOCK_CLOEXEC;
        if (fd_flags == fd_flags::nonblock) {
            flags |= SOCK_NONBLOCK;
        }
        return ::accept4(s, addr, addrlen, flags);
#endif
    }

    fdstat accept(fd_t s, fd_t& newfd, fd_flags fd_flags) noexcept {
        newfd = acceptEx(s, fd_flags, NULL, NULL);
        if (newfd == retired_fd) {
#if defined _WIN32
            return fdstat::failed;
#else
            if (errno != EAGAIN && errno != ECONNABORTED && errno != EPROTO && errno != EINTR) {
                return fdstat::failed;
            }
            else {
                return fdstat::wait;
            }
#endif
        }
        return fdstat::success;
    }

    status recv(fd_t s, int& rc, char* buf, int len) noexcept {
        rc = ::recv(s, buf, len, 0);
        if (rc == 0) {
            return status::close;
        }
        if (rc < 0) {
            return wait_finish() ? status::wait : status::failed;
        }
        return status::success;
    }

    status send(fd_t s, int& rc, const char* buf, int len) noexcept {
        int flags = 0;
#ifdef MSG_NOSIGNAL
        flags |= MSG_NOSIGNAL;
#endif
        rc = ::send(s, buf, len, flags);
        if (rc < 0) {
            return wait_finish() ? status::wait : status::failed;
        }
        return status::success;
    }

    expected<endpoint, status> recvfrom(fd_t s, int& rc, char* buf, int len) {
        endpoint ep;
        rc = ::recvfrom(s, buf, len, 0, ep.out_addr(), ep.out_addrlen());
        if (rc == 0) {
            return unexpected(status::close);
        }
        if (rc < 0) {
            return wait_finish() ? unexpected(status::wait)
                                 : unexpected(status::failed);
        }
        return ep;
    }

    status sendto(fd_t s, int& rc, const char* buf, int len, const endpoint& ep) noexcept {
        int flags = 0;
#ifdef MSG_NOSIGNAL
        flags |= MSG_NOSIGNAL;
#endif
        rc = ::sendto(s, buf, len, flags, ep.addr(), ep.addrlen());
        if (rc < 0) {
            return wait_finish() ? status::wait : status::failed;
        }
        return status::success;
    }

    std::optional<endpoint> getpeername(fd_t s) {
        endpoint ep;
        const int ok = ::getpeername(s, ep.out_addr(), ep.out_addrlen());
        if (!net_success(ok)) {
            return std::nullopt;
        }
        return ep;
    }

    std::optional<endpoint> getsockname(fd_t s) {
        endpoint ep;
        const int ok = ::getsockname(s, ep.out_addr(), ep.out_addrlen());
        if (!net_success(ok)) {
            return std::nullopt;
        }
        return ep;
    }

    bool unlink(const endpoint& ep) {
#if defined(_WIN32)
        if (!supportUnixDomainSocket()) {
            return false;
        }
#endif
        if (ep.family() != AF_UNIX) {
            return false;
        }
        auto [path, type] = ep.info();
        if (type != (uint16_t)un_format::pathname) {
            return false;
        }
#if defined _WIN32
        return win::unlink(win::u2w(path).c_str());
#else
        return 0 == ::unlink(path.c_str());
#endif
    }

    std::error_code errcode(fd_t s) noexcept {
        int err        = 0;
        socklen_t errl = sizeof(err);
        const int ok   = ::getsockopt(s, SOL_SOCKET, SO_ERROR, (char*)&err, &errl);
        if (net_success(ok)) {
            return std::error_code(err, get_error_category());
        }
        return std::error_code(get_error(), get_error_category());
    }

#if defined(_WIN32)
    bool unnamed_unix_bind(fd_t s) {
        char tmpdir[MAX_PATH];
        int bind_try = 0;
        for (;;) {
            switch (bind_try++) {
            case 0:
                GetTempPathA(MAX_PATH, tmpdir);
                break;
            case 1: {
                UINT n = GetWindowsDirectoryA(tmpdir, MAX_PATH);
                snprintf(tmpdir + n, MAX_PATH - n, "\\Temp\\");
                break;
            }
            case 2:
                snprintf(tmpdir, MAX_PATH, "C:\\Temp\\");
                break;
            case 3:
                tmpdir[0] = '.';
                tmpdir[1] = '\0';
                break;
            case 4:
                ::WSASetLastError(WSAEFAULT);
                return false;
            default:
                std::unreachable();
            }
            char tmpname[MAX_PATH];
            if (0 == GetTempFileNameA(tmpdir, "bee", 1, tmpname)) {
                continue;
            }
            auto ep = endpoint::from_unixpath(tmpname);
            socket::unlink(ep);
            if (ep.valid() && socket::bind(s, ep)) {
                return true;
            }
        }
        ::WSASetLastError(WSAEFAULT);
        return false;
    }
#endif

    bool pair(fd_t sv[2], fd_flags fd_flags) {
#if defined(_WIN32)
        const fd_t sfd = open(protocol::unix, fd_flags);
        if (sfd == retired_fd) {
            return false;
        }
        if (!unnamed_unix_bind(sfd)) {
            internal_close(sfd);
            return false;
        }
        if (!socket::listen(sfd, 5)) {
            internal_close(sfd);
            return false;
        }
        auto cep = getsockname(sfd);
        if (!cep) {
            internal_close(sfd);
            return false;
        }
        const fd_t cfd = open(protocol::unix, fd_flags);
        if (cfd == retired_fd) {
            internal_close(sfd);
            return false;
        }
        do {
            if (fdstat::failed == connect(cfd, *cep)) {
                break;
            }
            fd_set readfds, writefds, exceptfds;
            FD_ZERO(&readfds);
            FD_ZERO(&writefds);
            FD_ZERO(&exceptfds);
            FD_SET(sfd, &readfds);
            FD_SET(cfd, &writefds);
            FD_SET(cfd, &exceptfds);
            if (::select(0, &readfds, 0, 0, 0) <= 0) {
                break;
            }
            if (::select(0, 0, &writefds, &exceptfds, 0) <= 0) {
                break;
            }
            if (errcode(cfd)) {
                break;
            }
            fd_t newfd;
            if (fdstat::success != accept(sfd, newfd, fd_flags)) {
                break;
            }
            internal_close(sfd);
            sv[0] = newfd;
            sv[1] = cfd;
            return true;
        } while (false);
        internal_close(sfd);
        internal_close(cfd);
        return false;
#elif defined(__APPLE__)
        int temp[2];
        const int ok = ::socketpair(PF_UNIX, SOCK_STREAM, 0, temp);
        if (!net_success(ok)) {
            return false;
        }
        if (!set_cloexec(temp[0], true)) {
            goto fail;
        }
        if (!set_cloexec(temp[1], true)) {
            goto fail;
        }
        if (!set_nonblock(temp[0], fd_flags == fd_flags::nonblock)) {
            goto fail;
        }
        if (!set_nonblock(temp[1], fd_flags == fd_flags::nonblock)) {
            goto fail;
        }
        sv[0] = temp[0];
        sv[1] = temp[1];
        return true;
    fail:
        internal_close(temp[0]);
        internal_close(temp[1]);
        return false;
#else
        int flags = SOCK_STREAM | SOCK_CLOEXEC;
        if (fd_flags == fd_flags::nonblock) {
            flags |= SOCK_NONBLOCK;
        }
        const int ok = ::socketpair(PF_UNIX, flags, 0, sv);
        return net_success(ok);
#endif
    }

    bool pipe(fd_t sv[2], fd_flags fd_flags) {
#if defined(_WIN32)
        if (fd_flags == fd_flags::nonblock) {
            SetLastError(ERROR_INVALID_PARAMETER);
            return false;
        }
        SECURITY_ATTRIBUTES sa;
        sa.nLength              = sizeof(SECURITY_ATTRIBUTES);
        sa.bInheritHandle       = FALSE;
        sa.lpSecurityDescriptor = NULL;
        HANDLE rd = NULL, wr = NULL;
        if (!::CreatePipe(&rd, &wr, &sa, 0)) {
            return false;
        }
        sv[0] = (fd_t)rd;
        sv[1] = (fd_t)wr;
        return true;
#elif defined(__APPLE__)
        int temp[2];
        const int ok = ::pipe(temp);
        if (!net_success(ok)) {
            return false;
        }
        if (!set_cloexec(temp[0], true)) {
            goto fail;
        }
        if (!set_cloexec(temp[1], true)) {
            goto fail;
        }
        if (!set_nonblock(temp[0], fd_flags == fd_flags::nonblock)) {
            goto fail;
        }
        if (!set_nonblock(temp[1], fd_flags == fd_flags::nonblock)) {
            goto fail;
        }
        sv[0] = temp[0];
        sv[1] = temp[1];
        return true;
    fail:
        internal_close(temp[0]);
        internal_close(temp[1]);
        return false;
#else
        int flags = O_CLOEXEC;
        if (fd_flags == fd_flags::nonblock) {
            flags |= O_NONBLOCK;
        }
        const int ok = ::pipe2(sv, flags);
        return net_success(ok);
#endif
    }

    fd_t dup(fd_t s) noexcept {
#if defined(_WIN32)
        WSAPROTOCOL_INFOW shinfo;
        if (0 == ::WSADuplicateSocketW(s, ::GetCurrentProcessId(), &shinfo)) {
            return ::WSASocketW(FROM_PROTOCOL_INFO, FROM_PROTOCOL_INFO, FROM_PROTOCOL_INFO, &shinfo, 0, 0);
        }
        return retired_fd;
#else
        return ::dup(s);
#endif
    }
}
