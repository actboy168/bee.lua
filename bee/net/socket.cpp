#pragma once

#include <net/socket.h>
#include <assert.h>
#if defined _WIN32
#	include <Mstcpip.h>
#   include <bee/exception/windows_exception.h>
#   include <bee/utility/unicode.h>
#else
#	include <fcntl.h>
#	include <netinet/tcp.h>
#	include <signal.h>
#endif

namespace bee::net::socket {

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

#if defined _WIN32
	static int wsa_error_to_errno(int errcode)
	{
		switch (errcode) {
		//  10009 - File handle is not valid.
		case WSAEBADF:
			return EBADF;
		//  10013 - Permission denied.
		case WSAEACCES:
			return EACCES;
		//  10014 - Bad address.
		case WSAEFAULT:
			return EFAULT;
		//  10022 - Invalid argument.
		case WSAEINVAL:
			return EINVAL;
		//  10024 - Too many open files.
		case WSAEMFILE:
			return EMFILE;
		//  10035 - A non-blocking socket operation could not be completed immediately.
		case WSAEWOULDBLOCK:
			return EWOULDBLOCK;
		//  10036 - Operation now in progress.
		case WSAEINPROGRESS:
			return EAGAIN;
		//  10040 - Message too long.
		case WSAEMSGSIZE:
			return EMSGSIZE;
		//  10043 - Protocol not supported.
		case WSAEPROTONOSUPPORT:
			return EPROTONOSUPPORT;
		//  10047 - Address family not supported by protocol family.
		case WSAEAFNOSUPPORT:
			return EAFNOSUPPORT;
		//  10048 - Address already in use.
		case WSAEADDRINUSE:
			return EADDRINUSE;
		//  10049 - Cannot assign requested address.
		case WSAEADDRNOTAVAIL:
			return EADDRNOTAVAIL;
		//  10050 - Network is down.
		case WSAENETDOWN:
			return ENETDOWN;
		//  10051 - Network is unreachable.
		case WSAENETUNREACH:
			return ENETUNREACH;
		//  10052 - Network dropped connection on reset.
		case WSAENETRESET:
			return ENETRESET;
		//  10053 - Software caused connection abort.
		case WSAECONNABORTED:
			return ECONNABORTED;
		//  10054 - Connection reset by peer.
		case WSAECONNRESET:
			return ECONNRESET;
		//  10055 - No buffer space available.
		case WSAENOBUFS:
			return ENOBUFS;
		//  10057 - Socket is not connected.
		case WSAENOTCONN:
			return ENOTCONN;
		//  10060 - Connection timed out.
		case WSAETIMEDOUT:
			return ETIMEDOUT;
		//  10061 - Connection refused.
		case WSAECONNREFUSED:
			return ECONNREFUSED;
		//  10065 - No route to host.
		case WSAEHOSTUNREACH:
			return EHOSTUNREACH;
		default:
			net_assert(false);
		}
		//  Not reachable
		return 0;
	}
#endif

	static int error_no()
	{
#if defined _WIN32
		return wsa_error_to_errno(::WSAGetLastError());
#else
		return errno;
#endif
	}

	fd_t open(int family, int protocol)
	{
		switch (protocol) {
		case IPPROTO_TCP:
			return ::socket(family, SOCK_STREAM, protocol);
		case IPPROTO_UDP:
			return ::socket(family, SOCK_DGRAM, protocol);
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
		return ::close(s) != 0;
#endif
	}

	void shutdown(fd_t s)
	{
		assert(s != retired_fd);
#if defined _WIN32
		int rc = ::shutdown(s, SD_BOTH);
#else
		int rc = ::shutdown(s, SHUT_RDWR);
#endif
		net_assert_success(rc);
	}

	void shutdown_read(fd_t s)
	{
		assert(s != retired_fd);
#if defined _WIN32
		int rc = ::shutdown(s, SD_RECEIVE);
#else
		int rc = ::shutdown(s, SHUT_RD);
#endif
		net_assert_success(rc);
	}

	void shutdown_write(fd_t s)
	{
		assert(s != retired_fd);
#if defined _WIN32
		int rc = ::shutdown(s, SD_SEND);
#else
		int rc = ::shutdown(s, SHUT_WR);
#endif
		net_assert_success(rc);
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
#if defined _WIN32
		const int rc = setsockopt(s, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (char*) &flag, sizeof flag);
#else
		const int rc = setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char*) &flag, sizeof flag);
#endif
		net_assert_success(rc);
	}

	int connect(fd_t s, const endpoint& ep)
	{
		int rc = ::connect(s, ep.addr(), (int)ep.addrlen());
		if (rc == 0)
			return 0;

#if defined _WIN32
		const int error_code = ::WSAGetLastError();
		if (error_code == WSAEINPROGRESS || error_code == WSAEWOULDBLOCK)
			return -2;
#else
		if (errno == EINTR || errno == EINPROGRESS)
			return -2;
#endif
		return -1;
	}

	int bind(fd_t s, const endpoint& ep)
	{
		return ::bind(s, ep.addr(), ep.addrlen());
	}

	int listen(fd_t s, int backlog)
	{
		if (::listen(s, backlog) == -1)
		{
			return -2;
		}
		return 0;
	}

	int accept(fd_t s, fd_t& fd)
	{
		fd = ::accept(s, NULL, NULL);
		if (fd == retired_fd)
		{
#if defined _WIN32
			return -1;
#else 
			if (errno != EAGAIN && errno != ECONNABORTED && errno != EPROTO && errno != EINTR)
			{
				return -1;
			}
			else 
			{
				return -2;
			}
#endif
		}
		return 0;
	}

	int recv(fd_t s, char* buf, int len) {
		int rc = ::recv(s, buf, len, 0);
		if (rc == 0) {
			return -3;
		}
		if (rc < 0) {
			int ec = error_no();
			if (ec == EAGAIN || ec == EWOULDBLOCK || ec == EINTR) {
				return -2;
			}
			else {
				return -1;
			}
		}
		return rc;
	}

	int send(fd_t s, const char* buf, int len) {
		int rc = ::send(s, buf, len, 0);
		if (rc < 0) {
			int ec = error_no();
			if (ec == EAGAIN || ec == EWOULDBLOCK || ec == EINTR) {
				return -2;
			}
			else {
				return -1;
			}
		}
		return rc;
	}

	int  recvfrom(fd_t s, char* buf, int len, endpoint& ep) {
		socklen_t addrlen = ep.addrlen();
		int rc = ::recvfrom(s, buf, len, 0, ep.addr(), &addrlen);
		if (rc == 0) {
			return -3;
		}
		if (rc < 0) {
			int ec = error_no();
			if (ec == EAGAIN || ec == EWOULDBLOCK || ec == EINTR) {
				return -2;
			}
			else {
				return -1;
			}
		}
		ep.resize(addrlen);
		return rc;
	}

	int  sendto(fd_t s, const char* buf, int len, const endpoint& ep) {
		int rc = ::sendto(s, buf, len, 0, ep.addr(), ep.addrlen());
		if (rc < 0) {
			int ec = error_no();
			if (ec == EAGAIN || ec == EWOULDBLOCK || ec == EINTR) {
				return -2;
			}
			else {
				return -1;
			}
		}
		return rc;
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

	int errcode() {
#if defined _WIN32
		return ::WSAGetLastError();
#else
		return errno;
#endif
	}

	int errcode(fd_t fd) {
		int err;
		socklen_t errl = sizeof(err);
		if (getsockopt(fd, SOL_SOCKET, SO_ERROR, (char *)&err, &errl) >= 0) {
			return err;
		}
		return errcode();
	}

	std::string errmessage(int errcode) {
#if defined _WIN32
		return bee::w2u(bee::error_message(errcode));
#else
		return strerror(errcode);
#endif
	}

	std::string errmessage() {
#if defined _WIN32
		return errmessage(::WSAGetLastError());
#else
		return errmessage(errno);
#endif
	}
}
