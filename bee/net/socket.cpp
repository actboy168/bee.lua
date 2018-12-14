#if defined _WIN32
#    include <winsock2.h>
#    include <mswsock.h>
#    include <mstcpip.h>
#    include <bee/utility/unicode.h>
#    include <bee/net/unixsocket.h>
#else
#    include <fcntl.h>
#    include <netinet/tcp.h>
#    include <signal.h>
#    include <sys/types.h>
#    include <sys/socket.h>
#    include <netinet/in.h>
#    include <arpa/inet.h>
#    include <unistd.h>
#endif

#include <bee/net/socket.h>
#include <bee/net/endpoint.h>
#include <bee/error.h>
#include <assert.h>

#if defined _WIN32
#    define net_success(x) ((x) != SOCKET_ERROR)
#else
#    define net_success(x) ((x) == 0)
#endif

#define net_assert_success(x) \
    do { \
        auto r = (x); \
        assert(net_success(r)); \
    } while (0)


namespace bee::net::socket {

    static_assert(sizeof(SOCKET) == sizeof(fd_t));

    void initialize()
    {
        static bool initialized = false;
        if (!initialized) {
            initialized = true;
#if defined _WIN32
            WSADATA wd;
            int rc = WSAStartup(MAKEWORD(2, 2), &wd);
            assert(rc >= 0);
#else
            struct sigaction sa;
            sa.sa_handler = SIG_IGN;
            sigaction(SIGPIPE, &sa, 0);
#endif
        }
    }

    static bool wait_finish()
    {
        switch (last_neterror()) {
#if defined _WIN32
        case WSAEINPROGRESS:
        case WSAEWOULDBLOCK:
#else
        case EAGAIN:
        case EINTR:
        case EWOULDBLOCK:
#endif
            return true;
        default:
            return false;
        }
    }

    fd_t open(int family, protocol protocol)
    {
        switch (protocol) {
        case protocol::tcp:
            return ::socket(family, SOCK_STREAM, IPPROTO_TCP);
        case protocol::udp:
            return ::socket(family, SOCK_DGRAM, IPPROTO_UDP);
        case protocol::unix:
#if defined _WIN32
			if (u_enable()) {
				return ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			}
#endif
			return ::socket(family, SOCK_STREAM, 0);
        default:
            return retired_fd;
        }
    }

    bool close(fd_t s)
    {
        assert(s != retired_fd);
#if defined _WIN32
        return ::closesocket(s) != SOCKET_ERROR;
#else
        return ::close(s) == 0;
#endif
    }

    bool supportUnixDomainSocket() {
#if defined _WIN32
        fd_t fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
        bool ok = (fd != retired_fd);
        ::closesocket(fd);
        return ok;
#else
        return true;
#endif
    }

    bool shutdown(fd_t s, shutdown_flag flag)
    {
        assert(s != retired_fd);
#if defined(_WIN32)
        switch (flag) {
        case shutdown_flag::both:  return net_success(::shutdown(s, SD_BOTH));
        case shutdown_flag::read:  return net_success(::shutdown(s, SD_RECEIVE));
        case shutdown_flag::write: return net_success(::shutdown(s, SD_SEND));
        default: break;
        }
#else
        switch (flag) {
        case shutdown_flag::both:  return net_success(::shutdown(s, SHUT_RDWR));
        case shutdown_flag::read:  return net_success(::shutdown(s, SHUT_RD));
        case shutdown_flag::write: return net_success(::shutdown(s, SHUT_WR));
        default: break;
        }
#endif
        assert(false);
        return false;
    }

    void nonblocking(fd_t s)
    {
#if defined _WIN32
        unsigned long nonblock = 1;
        int rc = ioctlsocket(s, FIONBIO, &nonblock);
        net_assert_success(rc);
#else
        int flags = fcntl(s, F_GETFL, 0);
        if (flags == -1)
            flags = 0;
        int rc = fcntl(s, F_SETFL, flags | O_NONBLOCK);
        net_assert(rc != -1);
#endif
    }

    void keepalive(fd_t s, int keepalive, int keepalive_cnt, int keepalive_idle, int keepalive_intvl)
    {
#if defined _WIN32
        (void)keepalive_cnt;
        if (keepalive != -1)
        {
            tcp_keepalive keepaliveopts;
            keepaliveopts.onoff = keepalive;
            keepaliveopts.keepalivetime = keepalive_idle != -1 ? keepalive_idle * 1000 : 7200000;
            keepaliveopts.keepaliveinterval = keepalive_intvl != -1 ? keepalive_intvl * 1000 : 1000;
            DWORD num_bytes_returned;
            int rc = WSAIoctl(s, SIO_KEEPALIVE_VALS, &keepaliveopts, sizeof(keepaliveopts), NULL, 0, &num_bytes_returned, NULL, NULL);
            net_assert_success(rc);
        }
#else
        if (keepalive != -1)
        {
            int rc = setsockopt(s, SOL_SOCKET, SO_KEEPALIVE, (char*) &keepalive, sizeof (int));
            net_assert(rc == 0);

            if (keepalive_cnt != -1)
            {
                int rc = setsockopt(s, IPPROTO_TCP, TCP_KEEPCNT, &keepalive_cnt, sizeof (int));
                net_assert_success(rc);
            }
            if (keepalive_idle != -1)
            {
                int rc = setsockopt(s, IPPROTO_TCP, TCP_KEEPIDLE, &keepalive_idle, sizeof (int));
                net_assert_success(rc);
            }
            if (keepalive_intvl != -1)
            {
                int rc = setsockopt(s, IPPROTO_TCP, TCP_KEEPINTVL, &keepalive_intvl, sizeof (int));
                net_assert_success(rc);
            }
        }
#endif
    }

    void udp_connect_reset(fd_t s)
    {
#if defined _WIN32
        DWORD byte_retruned = 0;
        bool new_be = false;
        WSAIoctl(s, SIO_UDP_CONNRESET, &new_be, sizeof(new_be), NULL, 0, &byte_retruned, NULL, NULL);
#endif
    }

    void send_buffer(fd_t s, int bufsize)
    {
        const int rc = setsockopt(s, SOL_SOCKET, SO_SNDBUF, (char*) &bufsize, sizeof bufsize);
        net_assert_success(rc);
    }

    void recv_buffer(fd_t s, int bufsize)
    {
        const int rc = setsockopt(s, SOL_SOCKET, SO_RCVBUF, (char*) &bufsize, sizeof bufsize);
        net_assert_success(rc);
    }

    void reuse(fd_t s)
    {
        int flag = 1;
        const int rc = setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char*)&flag, sizeof flag);
        net_assert_success(rc);
    }

    status connect(fd_t s, const endpoint& ep)
    {
#if defined _WIN32
		if (u_enable() && ep.family() == AF_UNIX) {
			return u_connect(s, ep);
		}
#endif

        int rc = ::connect(s, ep.addr(), (int)ep.addrlen());
        if (rc == 0)
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

    status bind(fd_t s, const endpoint& ep)
    {
#if defined _WIN32
		if (u_enable() && ep.family() == AF_UNIX) {
			return u_bind(s, ep);
		}
#endif
        int ok = ::bind(s, ep.addr(), ep.addrlen());
        return ok == 0 ? status::success : status::failed;
    }

    status listen(fd_t s, int backlog)
    {
        if (::listen(s, backlog) == -1)
        {
            return status::failed;
        }
        return status::success;
    }

    status accept(fd_t s, fd_t& fd)
    {
        fd = ::accept(s, NULL, NULL);
        if (fd == retired_fd)
        {
#if defined _WIN32
            return status::failed;
#else 
            if (errno != EAGAIN && errno != ECONNABORTED && errno != EPROTO && errno != EINTR)
            {
                return status::failed;
            }
            else
            {
                return status::wait;
            }
#endif
        }
        return status::success;
    }

    status accept(fd_t s, fd_t& fd, endpoint& ep)
    {
        socklen_t addrlen = ep.addrlen();
        fd = ::accept(s, ep.addr(), &addrlen);
        if (fd == retired_fd)
        {
#if defined _WIN32
            return status::failed;
#else 
            if (errno != EAGAIN && errno != ECONNABORTED && errno != EPROTO && errno != EINTR)
            {
                return status::failed;
            }
            else 
            {
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

    status  recvfrom(fd_t s, int& rc, char* buf, int len, endpoint& ep) {
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

    status  sendto(fd_t s, int& rc, const char* buf, int len, const endpoint& ep) {
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

    bool unlink(fd_t s) {
        endpoint ep = endpoint::from_empty();
        if (!socket::getsockname(s, ep)) {
            return false;
        }
        if (ep.family() != AF_UNIX) {
            return false;
        }
        auto[path, port] = ep.info();
        if (path.size() == 0) {
            return false;
        }
#if defined _WIN32
        return !!::DeleteFileW(u2w(path).c_str());
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
        return last_neterror();
    }
}
