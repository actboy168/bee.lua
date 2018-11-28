#pragma once

#include <string>
#include <string_view>
#include <bee/utility/dynarray.h>
#include <bee/nonstd/expected.h>

struct sockaddr;

namespace bee::net {
#if defined(_WIN32)
	typedef int socklen_t;
#endif

	struct endpoint : private std::dynarray<std::byte> {
		static const size_t kMaxSize = 256;
		typedef std::dynarray<std::byte> mybase;
		std::pair<std::string, int> info() const;
		const sockaddr*             addr() const;
		socklen_t                   addrlen() const;
		sockaddr*                   addr();
		void                        resize(socklen_t len);

		static nonstd::expected<endpoint, std::string> from_hostname(const std::string_view& ip, int port);
		static endpoint                                from_empty();

	private:
		endpoint(size_t n);
	};
}
