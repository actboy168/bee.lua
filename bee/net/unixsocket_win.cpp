#include <winsock2.h>
#include <bee/net/unixsocket_win.h>
#include <bee/utility/unicode_win.h>
#include <fstream>
#include <charconv>
#include <limits>

namespace bee::net::socket {
	class file {
	public:
		static std::string read_all(const std::string& filename) {
			return file(filename, std::ios::in).read<std::string>();
		}
		static bool write_all(const std::string& filename, const std::string& value) {
			return file(filename, std::ios::out).write(value);
		}
	private:
		file(const std::string& filename, std::ios_base::openmode mode)
#if defined(_MSC_VER)
			: self(u2w(filename), std::ios::binary | mode)
#else
			: self(filename, std::ios::binary | mode)
#endif
		{ }
		operator bool() const {
			return !!self;
		}
		template <class SequenceT>
		SequenceT read() {
			if (!self) return SequenceT();
			return std::move(SequenceT((std::istreambuf_iterator<char>(self)), (std::istreambuf_iterator<char>())));
		}
		template <class SequenceT>
		bool write(SequenceT buf) {
			if (!self) return false;
			std::copy(buf.begin(), buf.end(), std::ostreambuf_iterator<char>(self));
			return true;
		}
		std::fstream self;
	};

	static bool read_tcp_port(const endpoint& ep, int& tcpport) {
		auto[path, type] = ep.info(); 
        if (type != 0) {
            return false;
        }
		auto unixpath = file::read_all(path);
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
		return file::write_all(path, portstr.data());
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
