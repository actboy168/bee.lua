#if defined _WIN32
#   include <winsock2.h>
#   include <mswsock.h>
#   include <mstcpip.h>
#   include <bee/platform/win/unicode.h>
#   include <bee/platform/win/version.h>
#   include <bee/platform/win/unlink.h>
#   include <fstream>
#   include <charconv>
#   include <limits>
#   include <array>
#else
#   include <fcntl.h>
#   include <netinet/tcp.h>
#   include <signal.h>
#   include <netinet/in.h>
#   include <unistd.h>
#   if defined(__APPLE__)
#       include <sys/ioctl.h>
#   endif
#endif

#include <bee/net/socket.h>
#include <bee/net/endpoint.h>
#include <bee/error.h>
#include <bee/nonstd/unreachable.h>
#include <assert.h>

#define net_success(x) ((x) == 0)

#if defined(__MINGW32__)
#define WSA_FLAG_NO_HANDLE_INHERIT 0x80
#endif

namespace bee::net::socket {
#if defined(_WIN32)
    static_assert(sizeof(SOCKET) == sizeof(fd_t));

#if defined(_MSC_VER)
#define FILENAME(n) win::u2w(n)
#else
#define FILENAME(n) (n)
#endif

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
        auto[path, type] = ep.info();
        if (type != (uint16_t)un_format::pathname) {
            return false;
        }
        auto unixpath = file_read(path);
        if (unixpath.empty()) {
            return false;
        }
        if (auto[p, ec] = std::from_chars(unixpath.data(), unixpath.data() + unixpath.size(), tcpport); ec != std::errc()) {
            return false;
        }
        if (tcpport <= 0 || tcpport > (std::numeric_limits<uint16_t>::max)()) {
            return false;
        }
        return true;
    }

    static bool write_tcp_port(const std::string& path, fd_t s) {
        endpoint newep = endpoint::from_empty();
        if (!socket::getsockname(s, newep)) {
            return false;
        }
        auto[ip, tcpport] = newep.info();
        std::array<char, 10> portstr;
        if (auto[p, ec] = std::to_chars(portstr.data(), portstr.data() + portstr.size() - 1, tcpport); ec != std::errc()) {
            return false;
        }
        else {
            p[0] = '\0';
        }
        return file_write(path, portstr.data());
    }

    static status u_connect(fd_t s, const endpoint& ep) {
        uint16_t tcpport = 0;
        if (!read_tcp_port(ep, tcpport)) {
            ::WSASetLastError(WSAECONNREFUSED);
            return status::failed;
        }
        auto newep = endpoint::from_hostname("127.0.0.1", tcpport);
        if (!newep.valid()) {
            ::WSASetLastError(WSAECONNREFUSED);
            return status::failed;
        }
        return socket::connect(s, newep);
    }

    static status u_bind(fd_t s, const endpoint& ep) {
        auto newep = endpoint::from_hostname("127.0.0.1", 0);
        if (!newep.valid()) {
            return status::failed;
        }
        status ok = socket::bind(s, newep);
        if (ok != status::success) {
            return ok;
        }
        auto[path, type] = ep.info();
        if (type != (uint16_t)un_format::pathname) {
            ::WSASetLastError(WSAENETDOWN);
            return status::failed;
        }
        if (!write_tcp_port(path, s)) {
            ::WSASetLastError(WSAENETDOWN);
            return status::failed;
        }
        return status::success;
    }

    static bool supportUnixDomainSocket_() {
        return !(bee::win::get_version() < bee::win::version {10, 0, 17763, 0});
    }
    static bool supportUnixDomainSocket() {
        static bool support = supportUnixDomainSocket_();
        return support;
    }
#endif

    bool initialize() {
        static bool initialized = false;
        if (!initialized) {
#if defined(_WIN32)
            WSADATA wd;
            int rc = WSAStartup(MAKEWORD(2, 2), &wd);
            initialized = rc >= 0;
#else
            initialized = true;
            struct sigaction sa;
            sa.sa_handler = SIG_IGN;
            sa.sa_flags = 0;
            sigemptyset(&sa.sa_mask);
            sigaction(SIGPIPE, &sa, 0);
#endif
        }
        return initialized;
    }

    static bool wait_finish() {
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
#if EAGAIN != EWOULDBLOCK
        case EWOULDBLOCK:
#endif
            return true;
        default:
            return false;
        }
#endif
    }

#if defined(_WIN32)
    static bool no_blocking(fd_t s) {
        unsigned long nonblock = 1;
        int ok = ioctlsocket(s, FIONBIO, &nonblock);
        return net_success(ok);
    }
    static bool no_inherit(fd_t s) {
        DWORD flags = 0;
        (void)flags;
        assert(::GetHandleInformation((HANDLE)s, &flags) && (flags & HANDLE_FLAG_INHERIT) == 0);
        return true;
    }
#elif defined(__APPLE__)
    static bool no_blocking(fd_t s) {
        int flags = fcntl(s, F_GETFL, 0);
        int ok = fcntl(s, F_SETFL, flags | O_NONBLOCK);
        return net_success(ok);
    }
    static bool no_inherit(fd_t s) {
        int ok;
        do
            ok = ioctl(s, FIOCLEX);
        while (!net_success(ok) && errno == EINTR);
        return net_success(ok);
    }
#endif

    static fd_t createSocket(int af, int type, int protocol) {
#if defined(_WIN32)
        fd_t fd = ::WSASocketW(af, type, protocol, 0, 0, WSA_FLAG_NO_HANDLE_INHERIT);
        if (fd != retired_fd) {
            if (!no_blocking(fd)) {
                close(fd);
                return retired_fd;
            }
        }
        return fd;
#elif defined(__APPLE__)
        fd_t fd = ::socket(af, type, protocol);
        if (fd != retired_fd) {
            if (!no_inherit(fd)) {
                close(fd);
                return retired_fd;
            }
            if (!no_blocking(fd)) {
                close(fd);
                return retired_fd;
            }
        }
        return fd;
#else
        return ::socket(af, type | SOCK_NONBLOCK | SOCK_CLOEXEC, protocol);
#endif
    }

    fd_t open(protocol protocol) {
        switch (protocol) {
        case protocol::tcp:
            return createSocket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
        case protocol::udp:
            return createSocket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
        case protocol::tcp6:
            return createSocket(PF_INET6, SOCK_STREAM, IPPROTO_TCP);
        case protocol::udp6:
            return createSocket(PF_INET6, SOCK_DGRAM, IPPROTO_UDP);
        case protocol::uds:
#if defined _WIN32
            if (!supportUnixDomainSocket()) {
                return createSocket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
            }
#endif
            return createSocket(PF_UNIX, SOCK_STREAM, 0);
        default:
            std::unreachable();
        }
    }

    bool close(fd_t s) {
#if defined _WIN32
        return ::closesocket(s) != SOCKET_ERROR;
#else
        return ::close(s) == 0;
#endif
    }

    bool shutdown(fd_t s, shutdown_flag flag) {
#if defined(_WIN32)
        switch (flag) {
        case shutdown_flag::both:  return net_success(::shutdown(s, SD_BOTH));
        case shutdown_flag::read:  return net_success(::shutdown(s, SD_RECEIVE));
        case shutdown_flag::write: return net_success(::shutdown(s, SD_SEND));
        default: std::unreachable();
        }
#else
        switch (flag) {
        case shutdown_flag::both:  return net_success(::shutdown(s, SHUT_RDWR));
        case shutdown_flag::read:  return net_success(::shutdown(s, SHUT_RD));
        case shutdown_flag::write: return net_success(::shutdown(s, SHUT_WR));
        default: std::unreachable();
        }
#endif
    }

    template <typename T>
    static bool setoption(fd_t s, int level, int optname, T& v) {
        int ok = setsockopt(s, level, optname, (char*)&v, sizeof(T));
        return net_success(ok);
    }

    bool setoption(fd_t s, option opt, int value) {
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

    void udp_connect_reset(fd_t s) {
#if defined _WIN32
        DWORD byte_retruned = 0;
        bool new_be = false;
        WSAIoctl(s, SIO_UDP_CONNRESET, &new_be, sizeof(new_be), NULL, 0, &byte_retruned, NULL, NULL);
#else
        (void)s;
#endif
    }

    status connect(fd_t s, const endpoint& ep) {
#if defined _WIN32
        if (!supportUnixDomainSocket() && ep.family() == AF_UNIX) {
            return u_connect(s, ep);
        }
#endif

        int ok = ::connect(s, ep.addr(), (int)ep.addrlen());
        if (net_success(ok))
            return status::success;

#if defined _WIN32
        const int error_code = ::WSAGetLastError();
        if (error_code == WSAEINPROGRESS || error_code == WSAEWOULDBLOCK)
            return status::wait;
#else
        if (errno == EINTR || errno == EINPROGRESS)
            return status::wait;
#endif
        return status::failed;
    }

    status bind(fd_t s, const endpoint& ep) {
#if defined _WIN32
        if (!supportUnixDomainSocket() && ep.family() == AF_UNIX) {
            return u_bind(s, ep);
        }
#endif
        int ok = ::bind(s, ep.addr(), ep.addrlen());
        return net_success(ok) ? status::success : status::failed;
    }

    status listen(fd_t s, int backlog) {
        int ok = ::listen(s, backlog);
        return net_success(ok) ? status::success : status::failed;
    }

    static fd_t acceptEx(fd_t s, struct sockaddr* addr, socklen_t* addrlen) {
#if defined(_WIN32) || defined(__APPLE__)
        fd_t fd = ::accept(s, addr, addrlen);
        if (fd != retired_fd) {
            if (!no_inherit(fd)) {
                close(fd);
                return retired_fd;
            }
            if (!no_blocking(fd)) {
                close(fd);
                return retired_fd;
            }
        }
        return fd;
#else
        return ::accept4(s, addr, addrlen, SOCK_NONBLOCK | SOCK_CLOEXEC);
#endif
    }

    status accept(fd_t s, fd_t& fd) {
        fd = acceptEx(s, NULL, NULL);
        if (fd == retired_fd) {
#if defined _WIN32
            return status::failed;
#else 
            if (errno != EAGAIN && errno != ECONNABORTED && errno != EPROTO && errno != EINTR) {
                return status::failed;
            }
            else {
                return status::wait;
            }
#endif
        }
        return status::success;
    }

    status accept(fd_t s, fd_t& fd, endpoint& ep) {
        socklen_t addrlen = ep.addrlen();
        fd = acceptEx(s, ep.addr(), &addrlen);
        if (fd == retired_fd) {
#if defined _WIN32
            return status::failed;
#else 
            if (errno != EAGAIN && errno != ECONNABORTED && errno != EPROTO && errno != EINTR) {
                return status::failed;
            }
            else {
                return status::wait;
            }
#endif
        }
        ep.resize(addrlen);
        return status::success;
    }

    status recv(fd_t s, int& rc, char* buf, int len) {
        rc = ::recv(s, buf, len, 0);
        if (rc == 0) {
            return status::close;
        }
        if (rc < 0) {
            return wait_finish() ? status::wait : status::failed;
        }
        return status::success;
    }

    status send(fd_t s, int& rc, const char* buf, int len) {
        rc = ::send(s, buf, len, 0);
        if (rc < 0) {
            return wait_finish() ? status::wait : status::failed;
        }
        return status::success;
    }

    status recvfrom(fd_t s, int& rc, char* buf, int len, endpoint& ep) {
        socklen_t addrlen = ep.addrlen();
        rc = ::recvfrom(s, buf, len, 0, ep.addr(), &addrlen);
        if (rc == 0) {
            return status::close;
        }
        if (rc < 0) {
            return wait_finish() ? status::wait : status::failed;
        }
        ep.resize(addrlen);
        return status::success;
    }

    status sendto(fd_t s, int& rc, const char* buf, int len, const endpoint& ep) {
        rc = ::sendto(s, buf, len, 0, ep.addr(), ep.addrlen());
        if (rc < 0) {
            return wait_finish() ? status::wait : status::failed;
        }
        return status::success;
    }

    bool getpeername(fd_t s, endpoint& ep) {
        socklen_t addrlen = ep.addrlen();
        if (::getpeername(s, ep.addr(), &addrlen) < 0) {
            return false;
        }
        ep.resize(addrlen);
        return true;
    }

    bool getsockname(fd_t s, endpoint& ep) {
        socklen_t addrlen = ep.addrlen();
        if (::getsockname(s, ep.addr(), &addrlen) < 0) {
            return false;
        }
        ep.resize(addrlen);
        return true;
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
        auto[path, type] = ep.info();
        if (type != (uint16_t)un_format::pathname) {
            return false;
        }
#if defined _WIN32
        return win::unlink(win::u2w(path).c_str());
#else
        return 0 == ::unlink(path.c_str());
#endif
    }

    int errcode(fd_t s) {
        int err;
        socklen_t errl = sizeof(err);
        if (getsockopt(s, SOL_SOCKET, SO_ERROR, (char *)&err, &errl) >= 0) {
            return err;
        }
#if defined(_WIN32)
        return ::WSAGetLastError();
#else
        return errno;
#endif
    }

#if !defined(_WIN32)
    bool blockpair(fd_t sv[2]) {
#if defined(__APPLE__)
        bool ok = 0 == ::socketpair(PF_UNIX, SOCK_STREAM, 0, sv);
        if (ok) {
            if (!no_inherit(sv[0])) {
                close(sv[0]);
                close(sv[1]);
                return false;
            }
            if (!no_inherit(sv[1])) {
                close(sv[0]);
                close(sv[1]);
                return false;
            }
        }
        return ok;
#else
        return 0 == ::socketpair(PF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sv);
#endif
    }
#endif

#if defined(_WIN32)
    bool unnamed_unix_bind(fd_t s) {
        char tmpdir[MAX_PATH];
        int bind_try = 0;
        for (;;) {
            switch (bind_try++) {
            case 0:
                GetTempPath(MAX_PATH, tmpdir);
                break;
            case 1: {
                UINT n = GetWindowsDirectory(tmpdir, MAX_PATH);
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
            }
            char tmpname[MAX_PATH];
            if (0 == GetTempFileNameA(tmpdir, "bee", 1, tmpname)) {
                continue;
            }
            auto ep = endpoint::from_unixpath(tmpname);
            socket::unlink(ep);
            if (ep.valid() && status::success == socket::bind(s, ep)) {
                return true;
            }
        }
        ::WSASetLastError(WSAEFAULT);
        return false;
    }
#endif

    bool pair(fd_t sv[2]) {
#if defined(_WIN32)
        fd_t sfd = retired_fd;
        fd_t cfd = retired_fd;
        auto cep = endpoint::from_empty();
        sfd = open(protocol::uds);
        if (sfd == retired_fd) {
            goto failed;
        }
        if (!unnamed_unix_bind(sfd)) {
            goto failed;
        }
        if (status::success != socket::listen(sfd, 5)) {
            goto failed;
        }
        if (!getsockname(sfd, cep)) {
            goto failed;
        }
        cfd = open(protocol::uds);
        if (cfd == retired_fd) {
            goto failed;
        }
        if (status::failed == connect(cfd, cep)) {
            goto failed;
        }
        fd_set readfds, writefds, exceptfds;
        FD_ZERO(&readfds);
        FD_ZERO(&writefds);
        FD_ZERO(&exceptfds);
        FD_SET(sfd, &readfds);
        FD_SET(cfd, &writefds);
        FD_SET(cfd, &exceptfds);
        if (::select(0, &readfds, 0, 0, 0) <= 0) {
            goto failed;
        }
        if (::select(0, 0, &writefds, &exceptfds, 0) <= 0) {
            goto failed;
        }
        if (0 != errcode(cfd)) {
            goto failed;
        }
        fd_t newfd;
        if (status::success != accept(sfd, newfd)) {
            goto failed;
        }
        close(sfd);
        sv[0] = newfd;
        sv[1] = cfd;
        return true;
    failed:
        int err = ::WSAGetLastError();
        if (sfd != retired_fd) close(sfd);
        if (cfd != retired_fd) close(cfd);
        ::WSASetLastError(err);
        return false;
#elif defined(__APPLE__)
        if (!blockpair(sv)) {
            return false;
        }
        if (!no_blocking(sv[0])) {
            close(sv[0]);
            close(sv[1]);
            return false;
        }
        if (!no_blocking(sv[1])) {
            close(sv[0]);
            close(sv[1]);
            return false;
        }
        return true;
#else
        return 0 == ::socketpair(PF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0, sv);
#endif
    }

    fd_t dup(fd_t s) {
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
