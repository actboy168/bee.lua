#if defined(_WIN32)
//  clang-format off
#    include <winsock2.h>
//  clang-format on
#    include <bee/net/uds_win.h>
#    include <bee/win/wtf8.h>
#    include <mstcpip.h>
#    include <mswsock.h>
#else
#    include <fcntl.h>
#    include <netinet/in.h>
#    include <netinet/tcp.h>
#    include <signal.h>
#    include <unistd.h>
#    if defined(__APPLE__)
#        include <sys/ioctl.h>
#    elif defined(__FreeBSD__) || defined(__OpenBSD__)
#        include <sys/socket.h>
#    endif
#endif

#include <bee/net/endpoint.h>
#include <bee/net/socket.h>
#include <bee/nonstd/unreachable.h>

namespace bee::net::socket {
    static bool net_success(int x) noexcept {
        return x == 0;
    }

#if defined(_WIN32)
    static_assert(sizeof(SOCKET) == sizeof(fd_t));

    static bool supportUnixDomainSocket() noexcept {
        static bool support = u_support();
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
#    if !defined(MSG_NOSIGNAL) && !defined(SO_NOSIGPIPE)
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
    static bool set_cloexec(fd_t s) noexcept {
        return true;
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
    static bool set_cloexec(fd_t fd) noexcept {
        int ok;
        do
            ok = ::ioctl(fd, FIOCLEX);
        while (!net_success(ok) && errno == EINTR);
        return net_success(ok);
    }
#endif

    bool close(fd_t s) noexcept {
#if defined(_WIN32)
        const int ok = ::closesocket(s);
#else
        const int ok = ::close(s);
#endif
        return net_success(ok);
    }

#if defined(_WIN32) || defined(__APPLE__) || defined(__FreeBSD__) || defined(SO_NOSIGPIPE)
    static void internal_close(fd_t s) noexcept {
#    if defined(_WIN32)
        auto saved = ::WSAGetLastError();
        close(s);
        ::WSASetLastError(saved);
#    else
        auto saved = errno;
        close(s);
        errno = saved;
#    endif
    }
#endif

    template <typename T>
    static bool setoption(fd_t s, int level, int optname, T& v) noexcept {
        const int ok = ::setsockopt(s, level, optname, (char*)&v, sizeof(T));
        return net_success(ok);
    }

    static fd_t createSocket(int af, int type, int protocol, fd_flags fd_flags) noexcept {
#if defined(_WIN32)
        const fd_t fd = u_createSocket(af, type, protocol, fd_flags);
        if (fd == retired_fd) {
            return retired_fd;
        }
        if (!set_nonblock(fd, fd_flags == fd_flags::nonblock)) {
            ::closesocket(fd);
            return retired_fd;
        }
        if (protocol == IPPROTO_UDP) {
            DWORD byte_retruned = 0;
            bool new_be         = false;
            ::WSAIoctl(fd, SIO_UDP_CONNRESET, &new_be, sizeof(new_be), NULL, 0, &byte_retruned, NULL, NULL);
        }
#elif defined(__APPLE__)
        const fd_t fd = ::socket(af, type, protocol);
        if (fd == retired_fd) {
            return retired_fd;
        }
        if (!set_cloexec(fd)) {
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

    fd_t open(protocol protocol, fd_flags flags) noexcept {
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
#if defined(_WIN32)
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

    bool bind(fd_t s, const endpoint& ep) noexcept {
#if defined(_WIN32)
        if (!supportUnixDomainSocket() && ep.addr()->sa_family == AF_UNIX) {
            return u_bind(s, ep);
        }
#endif
        if (ep.addr()->sa_family == AF_UNIX) {
            auto [type, path] = ep.get_unix();
            if (type == un_format::pathname) {
#if defined(_WIN32)
                ::DeleteFileW(wtf8::u2w(path).c_str());
#else
                ::unlink(path.data());
#endif
            }
        }
        const int ok = ::bind(s, ep.addr(), ep.addrlen());
        return net_success(ok);
    }

    bool listen(fd_t s, int backlog) noexcept {
        const int ok = ::listen(s, backlog);
        return net_success(ok);
    }

    status connect(fd_t s, const endpoint& ep) noexcept {
#if defined(_WIN32)
        if (!supportUnixDomainSocket() && ep.addr()->sa_family == AF_UNIX) {
            return u_connect(s, ep);
        }
#endif
        const int ok = ::connect(s, ep.addr(), ep.addrlen());
        if (net_success(ok)) {
            return status::success;
        }

#if defined(_WIN32)
        const int error_code = ::WSAGetLastError();
        if (error_code == WSAEINPROGRESS || error_code == WSAEWOULDBLOCK)
            return status::wait;
#else
        if (errno == EINTR || errno == EINPROGRESS)
            return status::wait;
#endif
        return status::failed;
    }

    static fd_t acceptEx(fd_t s, fd_flags fd_flags, struct sockaddr* addr, socklen_t* addrlen) noexcept {
#if defined(_WIN32) || defined(__APPLE__)
        const fd_t fd = ::accept(s, addr, addrlen);
        if (fd != retired_fd) {
            if (!set_cloexec(fd)) {
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

    status accept(fd_t s, fd_t& newfd, fd_flags fd_flags) noexcept {
        newfd = acceptEx(s, fd_flags, NULL, NULL);
        if (newfd == retired_fd) {
#if defined(_WIN32)
            return status::failed;
#else
            if (errno != EAGAIN && errno != ECONNABORTED && errno != EPROTO && errno != EINTR) {
                return status::failed;
            } else {
                return status::wait;
            }
#endif
        }
        return status::success;
    }

    recv_status recv(fd_t s, int& rc, char* buf, int len) noexcept {
        rc = ::recv(s, buf, len, 0);
        if (rc == 0) {
            return recv_status::close;
        }
        if (rc < 0) {
            return wait_finish() ? recv_status::wait : recv_status::failed;
        }
        return recv_status::success;
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

    status recvfrom(fd_t s, int& rc, endpoint& ep, char* buf, int len) noexcept {
        rc = ::recvfrom(s, buf, len, 0, ep.out_addr(), ep.out_addrlen());
        if (rc < 0) {
            return wait_finish() ? status::wait : status::failed;
        }
        return status::success;
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

    bool getpeername(fd_t s, endpoint& ep) noexcept {
        const int ok = ::getpeername(s, ep.out_addr(), ep.out_addrlen());
        return net_success(ok);
    }

    bool getsockname(fd_t s, endpoint& ep) noexcept {
        const int ok = ::getsockname(s, ep.out_addr(), ep.out_addrlen());
        return net_success(ok);
    }

    bool errcode(fd_t s, int& err) noexcept {
        socklen_t errlen = (socklen_t)sizeof(int);
        const int ok     = ::getsockopt(s, SOL_SOCKET, SO_ERROR, (char*)&err, &errlen);
        return net_success(ok);
    }

#if defined(_WIN32)
    static bool unnamed_unix_bind(fd_t s) noexcept {
        wchar_t buf[MAX_PATH];
        const wchar_t* tmpdir = buf;
        int bind_try          = 0;
        for (;;) {
            switch (bind_try++) {
            case 0:
                GetTempPathW(MAX_PATH, buf);
                tmpdir = buf;
                break;
            case 1: {
                constexpr size_t sz = std::size(L"\\Temp\\");
                UINT n              = GetWindowsDirectoryW(buf, MAX_PATH);
                if (n + sz >= MAX_PATH) {
                    tmpdir = nullptr;
                    break;
                }
                for (size_t i = 0; i < sz; ++i) {
                    buf[n + i] = L"\\Temp\\"[i];
                }
                tmpdir = buf;
                break;
            }
            case 2:
                tmpdir = L"C:\\Temp\\";
                break;
            case 3:
                tmpdir = L".";
                break;
            case 4:
                ::WSASetLastError(WSAEFAULT);
                return false;
            default:
                std::unreachable();
            }
            if (!tmpdir) {
                continue;
            }
            wchar_t tmpname[MAX_PATH];
            if (0 == GetTempFileNameW(tmpdir, L"bee", 1, tmpname)) {
                continue;
            }
            endpoint ep;
            if (endpoint::ctor_unix(ep, wtf8::w2u(tmpname).c_str()) && socket::bind(s, ep)) {
                return true;
            }
        }
        ::WSASetLastError(WSAEFAULT);
        return false;
    }
#endif

    bool pair(fd_t sv[2], fd_flags fd_flags) noexcept {
#if defined(_WIN32)
        auto proto     = supportUnixDomainSocket() ? protocol::unix : protocol::tcp;
        const fd_t sfd = open(proto, fd_flags);
        if (sfd == retired_fd) {
            return false;
        }
        if (proto == protocol::unix) {
            if (!unnamed_unix_bind(sfd)) {
                internal_close(sfd);
                return false;
            }
        } else {
            if (!socket::bind(sfd, endpoint::from_localhost(0))) {
                internal_close(sfd);
                return false;
            }
        }
        if (!socket::listen(sfd, 5)) {
            internal_close(sfd);
            return false;
        }
        endpoint cep;
        if (!getsockname(sfd, cep)) {
            internal_close(sfd);
            return false;
        }
        const fd_t cfd = open(proto, fd_flags);
        if (cfd == retired_fd) {
            internal_close(sfd);
            return false;
        }
        do {
            if (status::failed == connect(cfd, cep)) {
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
            int err = 0;
            if (!errcode(cfd, err) || err != 0) {
                break;
            }
            fd_t newfd;
            if (status::success != accept(sfd, newfd, fd_flags)) {
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
        if (!set_cloexec(temp[0])) {
            goto fail;
        }
        if (!set_cloexec(temp[1])) {
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

    bool pipe(fd_t sv[2], fd_flags fd_flags) noexcept {
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
        if (!set_cloexec(temp[0])) {
            goto fail;
        }
        if (!set_cloexec(temp[1])) {
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
