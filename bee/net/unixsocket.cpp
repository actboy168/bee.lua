#include <winsock2.h>
#include <bee/net/unixsocket.h>
#include <bee/utility/unicode.h>
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
		auto[path, port] = ep.info();
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

	static void write_tcp_port(const endpoint& ep, fd_t s) {
		endpoint newep = endpoint::from_empty();
		if (!socket::getsockname(s, newep)) {
			return;
		}
		auto[ip, tcpport] = newep.info();
		std::array<char, 10> portstr;
		if (auto[p, ec] = std::to_chars(portstr.data(), portstr.data() + portstr.size() - 1, tcpport); ec != std::errc()) {
			return;
		}
		else {
			p[0] = '\0';
		}
		auto[path, port] = ep.info();
		file::write_all(path, portstr.data());
	}

	bool u_enable() {
		return true;
	}

	int u_connect(fd_t s, const endpoint& ep) {
		int tcpport = 0;
		if (!read_tcp_port(ep, tcpport)) {
			::WSASetLastError(WSAECONNREFUSED);
			return - 1;
		}
		auto newep = endpoint::from_hostname("127.0.0.1", tcpport);
		if (!newep) {
			::WSASetLastError(WSAECONNREFUSED);
			return -1;
		}
		return socket::connect(s, *newep);
	}
	int u_bind(fd_t s, const endpoint& ep) {
		auto newep = endpoint::from_hostname("127.0.0.1", 0);
		if (!newep) {
			::WSASetLastError(0);
			return -1;
		}
		int ok = socket::bind(s, *newep);
		if (ok < 0) {
			return ok;
		}
		write_tcp_port(ep, s);
		return ok;
	}
}
