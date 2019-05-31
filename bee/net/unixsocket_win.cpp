#include <winsock2.h>
#include <bee/net/unixsocket_win.h>
#include <bee/utility/unicode_win.h>
#include <fstream>
#include <charconv>
#include <limits>

namespace bee::net::socket {
#if defined(_MSC_VER)
#define FILENAME(n) u2w(n)
#else
#define FILENAME(n) (n)
#endif
	static std::string file_read(const std::string& filename) {
		std::fstream fs(FILENAME(filename), std::ios::binary | std::ios::in);
		if (!fs) return std::string();
		return std::string((std::istreambuf_iterator<char>(fs)), (std::istreambuf_iterator<char>()));
	}
	static bool file_write(const std::string& filename, const std::string& value) {
		std::fstream fs(FILENAME(filename), std::ios::binary | std::ios::out);
		if (!fs) return false;
		std::copy(value.begin(), value.end(), std::ostreambuf_iterator<char>(fs));
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

    status u_connect(fd_t s, const endpoint& ep) {
		int tcpport = 0;
		if (!read_tcp_port(ep, tcpport)) {
			::WSASetLastError(WSAECONNREFUSED);
			return status::failed;
		}
		auto newep = endpoint::from_hostname("127.0.0.1", tcpport);
		if (!newep) {
			::WSASetLastError(WSAECONNREFUSED);
			return status::failed;
		}
		return socket::connect(s, *newep);
	}
    status u_bind(fd_t s, const endpoint& ep) {
		auto newep = endpoint::from_hostname("127.0.0.1", 0);
		if (!newep) {
			return status::failed;
		}
		status ok = socket::bind(s, *newep);
		if (ok != status::success) {
			return ok;
		}
		if (!write_tcp_port(ep, s)) {
			::WSASetLastError(WSAENETDOWN);
            return status::failed;
		}
		return status::success;
	}
}
