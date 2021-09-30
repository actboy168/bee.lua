#if defined _WIN32
#   include <winsock2.h>
#   include <mswsock.h>
#   include <mstcpip.h>
#   include <bee/utility/unicode_win.h>
#   include <bee/platform/version.h>
#   include <fstream>
#   include <charconv>
#   include <limits>
#   include <array>
#   include <map>
#   include <optional>
#   include <mutex>
#   include <any>
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
#include <assert.h>

#if defined(_WIN32)
#    define net_success(x) ((x) != SOCKET_ERROR)
#else
#    define net_success(x) ((x) == 0)
#endif

#if defined(NDEBUG)
#define net_assert_success(x) ((void)x)
#else
#define net_assert_success(x) assert(net_success(x))
#endif


#if defined(__MINGW32__)
#define WSA_FLAG_NO_HANDLE_INHERIT 0x80
#endif

namespace bee::net::socket {
#if defined(_WIN32)
    static_assert(sizeof(SOCKET) == sizeof(fd_t));
#endif

#if defined _WIN32

#if defined(_MSC_VER)
#define FILENAME(n) u2w(n)
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

	static bool read_tcp_port(const endpoint& ep, int& tcpport) {
		auto[path, type] = ep.info(); 
        if (type != 0) {
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

	static bool write_tcp_port(const endpoint& ep, fd_t s) {
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
		auto[path, type] = ep.info();
        if (type != 0) {
            return true;
        }
		return file_write(path, portstr.data());
	}

    namespace state {
        typedef std::map<std::string_view, std::any> u_table_t;
        std::map<fd_t, u_table_t> _state;
        std::mutex                _mutex;

        static void remove(fd_t s) {
            std::unique_lock<std::mutex> _(_mutex);
            _state.erase(s);
        }

        template <typename T>
        static void store(fd_t s, std::string_view key, T const& value) {
            std::unique_lock<std::mutex> _(_mutex);
            _state[s].emplace(key, value);
        }

        template <typename T>
        static std::optional<T> load(fd_t s, std::string_view key) {
            std::unique_lock<std::mutex> _(_mutex);
            auto i1 = _state.find(s);
            if (i1 == _state.end()) {
                return {};
            }
            u_table_t& table = i1->second;
            auto i2 = table.find(key);
            if (i2 == table.end()) {
                return {};
            }
            std::any& value = i2->second;
            T* r = std::any_cast<T>(&value);
            if (!r) {
                return {};
            }
            return *r;
        }
    }

    static status u_connect(fd_t s, const endpoint& ep) {
		int tcpport = 0;
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
		if (!write_tcp_port(ep, s)) {
			::WSASetLastError(WSAENETDOWN);
            return status::failed;
		}
        auto[path, type] = ep.info();
        state::store<std::string>(s, "path", path);
        return status::success;
    }
#endif

    void initialize()
    {
        static bool initialized = false;
        if (!initialized) {
            initialized = true;
#if defined(_WIN32)
            WSADATA wd;
            int rc = WSAStartup(MAKEWORD(2, 2), &wd);
            (void)rc;
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
#if defined(_WIN32)
        case WSAEINPROGRESS:
        case WSAEWOULDBLOCK:
#else
        case EAGAIN:
        case EINTR:
#if EAGAIN != EWOULDBLOCK
        case EWOULDBLOCK:
#endif
#endif
            return true;
        default:
            return false;
        }
    }

#if defined(_WIN32)
    static void no_blocking(fd_t s) {
        unsigned long nonblock = 1;
        int rc = ioctlsocket(s, FIONBIO, &nonblock);
        net_assert_success(rc);
    }
    static bool no_inherit(fd_t s) {
        DWORD flags = 0;
        (void)flags;
        assert(::GetHandleInformation((HANDLE)s, &flags) && (flags & HANDLE_FLAG_INHERIT) == 0);
        return true;
    }
#elif defined(__APPLE__)
    static void no_blocking(fd_t s) {
        int flags = fcntl(s, F_GETFL, 0);
        int rc = fcntl(s, F_SETFL, flags | O_NONBLOCK);
        net_assert_success(rc);
    }
    static bool no_inherit(fd_t s) {
        int r;
        do
            r = ioctl(s, FIOCLEX);
        while (r == -1 && errno == EINTR);
        return !r;
    }
#endif

    static fd_t createSocket(int af, int type, int protocol) {
#if defined(_WIN32)
        fd_t fd = ::WSASocketW(af, type, protocol, 0, 0, WSA_FLAG_NO_HANDLE_INHERIT);
        if (fd != retired_fd) {
            no_blocking(fd);
        }
        return fd;
#elif defined(__APPLE__)
        fd_t fd = ::socket(af, type, protocol);
        if (fd != retired_fd) {
            no_inherit(fd);
            no_blocking(fd);
        }
        return fd;
#else
        return ::socket(af, type | SOCK_NONBLOCK | SOCK_CLOEXEC, protocol);
#endif
    }

    fd_t open(protocol protocol)
    {
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

#if defined _WIN32
    static bool forceSimulationUDS = false;
    static bool supportUnixDomainSocket_() {
        return !(bee::platform::get_version() < bee::platform::version {10, 0, 17763, 0});
    }
    bool supportUnixDomainSocket() {
        if (forceSimulationUDS) {
            return false;
        }
        static bool support = supportUnixDomainSocket_();
        return support;
    }
    void simulationUnixDomainSocket(bool open) {
        forceSimulationUDS = open;
    }
#endif

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

    template <typename T>
    static void setoption(fd_t s, int level, int optname, T& v) {
        const int rc = setsockopt(s, level, optname, (char*)&v, sizeof(T));
        net_assert_success(rc);
    }

    void setoption(fd_t s, option opt, int value) {
        switch (opt) {
        case option::reuseaddr:
            setoption(s, SOL_SOCKET, SO_REUSEADDR, value);
            break;
        case option::sndbuf:
            setoption(s, SOL_SOCKET, SO_SNDBUF, value);
            break;
        case option::rcvbuf:
            setoption(s, SOL_SOCKET, SO_RCVBUF, value);
            break;
        }
    }

    void keepalive(fd_t s, int keepalive, int keepalive_cnt, int keepalive_idle, int keepalive_intvl)
    {
        // TODO
        if (keepalive == -1) {
            return;
        }
#if defined(_WIN32)
        (void)keepalive_cnt;
        tcp_keepalive keepaliveopts;
        keepaliveopts.onoff = keepalive;
        keepaliveopts.keepalivetime = keepalive_idle != -1 ? keepalive_idle * 1000 : 7200000;
        keepaliveopts.keepaliveinterval = keepalive_intvl != -1 ? keepalive_intvl * 1000 : 1000;
        DWORD num_bytes_returned;
        int rc = WSAIoctl(s, SIO_KEEPALIVE_VALS, &keepaliveopts, sizeof(keepaliveopts), NULL, 0, &num_bytes_returned, NULL, NULL);
        net_assert_success(rc);
#elif defined(__linux__)
        int rc = setsockopt(s, SOL_SOCKET, SO_KEEPALIVE, (char*) &keepalive, sizeof (int));
        net_assert_success(rc);
        if (keepalive_cnt != -1) {
            int rc = setsockopt(s, IPPROTO_TCP, TCP_KEEPCNT, &keepalive_cnt, sizeof (int));
            net_assert_success(rc);
        }
        if (keepalive_idle != -1) {
            int rc = setsockopt(s, IPPROTO_TCP, TCP_KEEPIDLE, &keepalive_idle, sizeof (int));
            net_assert_success(rc);
        }
        if (keepalive_intvl != -1) {
            int rc = setsockopt(s, IPPROTO_TCP, TCP_KEEPINTVL, &keepalive_intvl, sizeof (int));
            net_assert_success(rc);
        }
#elif defined(__apple__)
        (void)keepalive_cnt;
        (void)keepalive_intvl;
        int rc = setsockopt(s, SOL_SOCKET, SO_KEEPALIVE, (char*) &keepalive, sizeof (int));
        net_assert_success(rc);
        if (keepalive_idle != -1) {
            int rc = setsockopt(s, IPPROTO_TCP, TCP_KEEPALIVE, &keepalive_idle, sizeof (int));
            net_assert_success(rc);
        }
#endif
    }

    void udp_connect_reset(fd_t s)
    {
#if defined _WIN32
        DWORD byte_retruned = 0;
        bool new_be = false;
        WSAIoctl(s, SIO_UDP_CONNRESET, &new_be, sizeof(new_be), NULL, 0, &byte_retruned, NULL, NULL);
#else
        (void)s;
#endif
    }

    status connect(fd_t s, const endpoint& ep)
    {
#if defined _WIN32
		if (!supportUnixDomainSocket() && ep.family() == AF_UNIX) {
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
		if (!supportUnixDomainSocket() && ep.family() == AF_UNIX) {
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

    static fd_t acceptEx(fd_t s, struct sockaddr* addr, socklen_t* addrlen) {
#if defined(_WIN32) || defined(__APPLE__)
        fd_t fd = ::accept(s, addr, addrlen);
        if (fd != retired_fd) {
            no_inherit(fd);
            no_blocking(fd);
        }
        return fd;
#else
        return ::accept4(s, addr, addrlen, SOCK_NONBLOCK | SOCK_CLOEXEC);
#endif
    }

    status accept(fd_t s, fd_t& fd)
    {
        fd = acceptEx(s, NULL, NULL);
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
        fd = acceptEx(s, ep.addr(), &addrlen);
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
#if defined(_WIN32)
        if (!socket::supportUnixDomainSocket()) {
            auto path = state::load<std::string>(s, "path");
            if (!path) {
                return false;
            }
            state::remove(s);
            return 0 == _wunlink(u2w(path.value()).c_str());
        }
#endif
        endpoint ep = endpoint::from_empty();
        if (!socket::getsockname(s, ep)) {
            return false;
        }
        return unlink(ep);
    }

    bool unlink(const endpoint& ep) {
        if (ep.family() != AF_UNIX) {
            return false;
        }
        auto[path, type] = ep.info();
        if (path.size() == 0 || type != 0) {
            return false;
        }
#if defined _WIN32
        return 0 == _wunlink(u2w(path).c_str());
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

#if !defined(_WIN32)
    bool blockpair(fd_t sv[2]) {
#if defined(__APPLE__)
        bool ok = 0 == ::socketpair(PF_UNIX, SOCK_STREAM, 0, sv);
        if (ok) {
            no_inherit(sv[0]);
            no_inherit(sv[1]);
        }
        return ok;
#else
        return 0 == ::socketpair(PF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sv);
#endif
    }
#endif

    bool pair(fd_t sv[2]) {
#if defined(_WIN32)
        fd_t sfd = retired_fd;
        fd_t cfd = retired_fd;
        auto cep = endpoint::from_empty();
        auto sep = endpoint::from_hostname("127.0.0.1", 0);
        if (!sep.valid()) {
            goto failed;
        }
        sfd = open(protocol::tcp);
        if (sfd == retired_fd) {
            goto failed;
        }
        if (status::success != socket::bind(sfd, sep)) {
            goto failed;
        }
        if (status::success != socket::listen(sfd, 5)) {
            goto failed;
        }
        if (!getsockname(sfd, cep)) {
            goto failed;
        }
        cfd = open(protocol::tcp);
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
        no_blocking(sv[0]);
        no_blocking(sv[1]);
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
