#include <lua.hpp>
#include <bee/lua/binding.h>
#include <bee/net/socket.h>
#include <bee/net/endpoint.h>
#include <limits>

#if _MSC_VER > 0
#define strcasecmp _stricmp
#endif

namespace luanet {
	using namespace bee::net;

	static const int kDefaultBackLog = 5;

	static int push_neterror(lua_State* L, const char* msg) {
		lua_pushnil(L);
		lua_pushfstring(L, "%s: (%d)%s", msg, socket::errcode(), socket::errmessage().c_str());
		return 2;
	}
	static int lclose(lua_State* L, socket::fd_t fd) {
		if (!socket::close(fd)) {
			return push_neterror(L, "close");
		}
		lua_pushboolean(L, 1);
		return 1;
	}
	static int lrecv(lua_State* L, socket::fd_t fd) {
		lua_Integer len = luaL_optinteger(L, 2, LUAL_BUFFERSIZE);
		if (len > (std::numeric_limits<int>::max)()) {
			return luaL_error(L, "bad argument #1 to 'recv' (invalid number)");
		}
		luaL_Buffer b;
		luaL_buffinit(L, &b);
		char* buf = luaL_prepbuffsize(&b, (size_t)len);
		int rc = socket::recv(fd, buf, (int)len);
		switch (rc) {
		case -3:
			lua_pushnil(L);
			return 1;
		case -2:
			lua_pushboolean(L, 0);
			return 1;
		case -1:
			return push_neterror(L, "recv");
		default:
			luaL_pushresultsize(&b, rc);
			return 1;
		}
	}
	static int lsend(lua_State* L, socket::fd_t fd) {
		size_t len;
		const char* buf = luaL_checklstring(L, 2, &len);
		int rc = socket::send(fd, buf, (int)len);
		switch (rc) {
		case -2:
			lua_pushboolean(L, 0);
			return 1;
		case -1:
			return push_neterror(L, "send");
		default:
			lua_pushinteger(L, rc);
			return 1;
		}
	}

	namespace tcp::stream {
		static int recv(lua_State* L) {
			return lrecv(L, *(socket::fd_t*)luaL_checkudata(L, 1, "bee::tcp::stream"));
		}
		static int send(lua_State* L) {
			return lsend(L, *(socket::fd_t*)luaL_checkudata(L, 1, "bee::tcp::stream"));
		}
		static int close(lua_State* L) {
			return lclose(L, *(socket::fd_t*)luaL_checkudata(L, 1, "bee::tcp::stream"));
		}
		static void constructor(lua_State* L, socket::fd_t fd) {
			socket::fd_t* pfd = (socket::fd_t*)lua_newuserdata(L, sizeof(socket::fd_t));
			*pfd = fd;
			if (luaL_newmetatable(L, "bee::tcp::stream")) {
				luaL_Reg mt[] = {
					{ "recv", recv },
					{ "send", send },
					{ "close", close },
					{ "__gc", close },
					{ NULL, NULL },
				};
				luaL_setfuncs(L, mt, 0);
				lua_pushvalue(L, -1);
				lua_setfield(L, -2, "__index");
			}
			lua_setmetatable(L, -2);
			socket::reuse(fd);
			socket::nonblocking(fd);
		}
	}

	namespace tcp::listen {
		static int accept(lua_State* L) {
			socket::fd_t& fd = *(socket::fd_t*)luaL_checkudata(L, 1, "bee::tcp::listen");
			socket::fd_t newfd;
			if (socket::accept(fd, newfd)) {
				return push_neterror(L, "accept");
			}
			tcp::stream::constructor(L, newfd);
			return 1;
		}
		static int close(lua_State* L) {
			return lclose(L, *(socket::fd_t*)luaL_checkudata(L, 1, "bee::tcp::listen"));
		}
		static void constructor(lua_State* L, socket::fd_t fd) {
			socket::fd_t* pfd = (socket::fd_t*)lua_newuserdata(L, sizeof(socket::fd_t));
			*pfd = fd;
			if (luaL_newmetatable(L, "bee::tcp::listen")) {
				luaL_Reg mt[] = {
					{ "accept", accept },
					{ "close", close },
					{ "__gc", close },
					{ NULL, NULL },
				};
				luaL_setfuncs(L, mt, 0);
				lua_pushvalue(L, -1);
				lua_setfield(L, -2, "__index");
			}
			lua_setmetatable(L, -2);
			socket::reuse(fd);
			socket::nonblocking(fd);
		}
	}

	namespace udp::client {
		static int recv(lua_State* L) {
			return lrecv(L, *(socket::fd_t*)luaL_checkudata(L, 1, "bee::udp::client"));
		}
		static int send(lua_State* L) {
			return lsend(L, *(socket::fd_t*)luaL_checkudata(L, 1, "bee::udp::client"));
		}
		static int close(lua_State* L) {
			return lclose(L, *(socket::fd_t*)luaL_checkudata(L, 1, "bee::udp::client"));
		}
		static void constructor(lua_State* L, socket::fd_t fd) {
			socket::fd_t* pfd = (socket::fd_t*)lua_newuserdata(L, sizeof(socket::fd_t));
			*pfd = fd;
			if (luaL_newmetatable(L, "bee::udp::client")) {
				luaL_Reg mt[] = {
					{ "recv", recv },
					{ "send", send },
					{ "close", close },
					{ "__gc", close },
					{ NULL, NULL },
				};
				luaL_setfuncs(L, mt, 0);
				lua_pushvalue(L, -1);
				lua_setfield(L, -2, "__index");
			}
			lua_setmetatable(L, -2);
			socket::reuse(fd);
			socket::nonblocking(fd);
		}
	}

	namespace udp::server {
		static int recvfrom(lua_State* L) {
			socket::fd_t& fd = *(socket::fd_t*)luaL_checkudata(L, 1, "bee::udp::server");
			lua_Integer len = luaL_optinteger(L, 2, LUAL_BUFFERSIZE);
			if (len > (std::numeric_limits<int>::max)()) {
				return luaL_error(L, "bad argument #1 to 'recv' (invalid number)");
			}
			endpoint ep = endpoint::from_empty();
			luaL_Buffer b;
			luaL_buffinit(L, &b);
			char* buf = luaL_prepbuffsize(&b, (size_t)len);
			int rc = socket::recvfrom(fd, buf, (int)len, ep);
			switch (rc) {
			case -3:
				lua_pushnil(L);
				return 1;
			case -2:
				lua_pushboolean(L, 0);
				return 1;
			case -1:
				return push_neterror(L, "recv");
			default: {
				luaL_pushresultsize(&b, rc);
				auto [ip, port] = ep.info();
				lua_pushlstring(L, ip.data(), ip.size());
				lua_pushinteger(L, port);
				return 3;
			}
			}
		}
		static int sendto(lua_State* L) {
			socket::fd_t& fd = *(socket::fd_t*)luaL_checkudata(L, 1, "bee::udp::server");
			size_t len;
			const char* buf = luaL_checklstring(L, 2, &len);
			std::string_view ip = bee::lua::to_strview(L, 3);
			int port = (int)luaL_checkinteger(L, 4);
			auto ep = bee::net::endpoint::from_hostname(ip, port);
			if (!ep) {
				lua_pushlstring(L, ep.error().data(), ep.error().size());
				return lua_error(L);
			}
			int rc = socket::sendto(fd, buf, (int)len, ep.value());
			switch (rc) {
			case -2:
				lua_pushboolean(L, 0);
				return 1;
			case -1:
				return push_neterror(L, "send");
			default:
				lua_pushinteger(L, rc);
				return 1;
			}
		}
		static int close(lua_State* L) {
			return lclose(L, *(socket::fd_t*)luaL_checkudata(L, 1, "bee::udp::server"));
		}
		static void constructor(lua_State* L, socket::fd_t fd) {
			socket::fd_t* pfd = (socket::fd_t*)lua_newuserdata(L, sizeof(socket::fd_t));
			*pfd = fd;
			if (luaL_newmetatable(L, "bee::udp::server")) {
				luaL_Reg mt[] = {
					{ "recvfrom", recvfrom },
					{ "send", sendto },
					{ "close", close },
					{ "__gc", close },
					{ NULL, NULL },
				};
				luaL_setfuncs(L, mt, 0);
				lua_pushvalue(L, -1);
				lua_setfield(L, -2, "__index");
			}
			lua_setmetatable(L, -2);
			socket::reuse(fd);
			socket::nonblocking(fd);
		}
	}

	static int read_protocol(lua_State* L, int idx) {
		const char* type = luaL_checkstring(L, 1);
		if (0 == strcasecmp(type, "tcp")) {
			return IPPROTO_TCP;
		}
		else if (0 == strcasecmp(type, "udp")) {
			return IPPROTO_UDP;
		}
		else {
			return luaL_error(L, "invalid protocol `%s`.", type);
		}
	}
	static int connect(lua_State* L) {
		int protocol = read_protocol(L, 1);
		std::string_view ip = bee::lua::to_strview(L, 2);
		int port = (int)luaL_checkinteger(L, 3);
		auto ep = bee::net::endpoint::from_hostname(ip, port);
		if (!ep) {
			lua_pushlstring(L, ep.error().data(), ep.error().size());
			return lua_error(L);
		}
		socket::fd_t fd = socket::open(ep->addr()->sa_family, protocol);
		if (fd == socket::retired_fd) {
			return push_neterror(L, "socket");
		}
		if (protocol == IPPROTO_TCP) {
			tcp::stream::constructor(L, fd);
		}
		else {
			udp::client::constructor(L, fd);
		}
		switch (socket::connect(fd, *ep)) {
		case 0:
			lua_pushboolean(L, true);
			return 2;
		case -2:
			lua_pushboolean(L, false);
			return 2;
		case -1:
		default:
			return push_neterror(L, "connect");
		}
	}
	static int bind(lua_State* L) {
		int protocol = read_protocol(L, 1);
		std::string_view ip = bee::lua::to_strview(L, 2);
		int port = (int)luaL_checkinteger(L, 3);
		auto ep = bee::net::endpoint::from_hostname(ip, port);
		if (!ep) {
			lua_pushnil(L);
			lua_pushlstring(L, ep.error().data(), ep.error().size());
			return 2;
		}
		socket::fd_t fd = socket::open(ep->addr()->sa_family, protocol);
		if (fd == socket::retired_fd) {
			return push_neterror(L, "socket");
		}
		if (protocol == IPPROTO_TCP) {
			tcp::listen::constructor(L, fd);
		}
		else {
			udp::server::constructor(L, fd);
		}
		socket::reuse(fd);
		socket::nonblocking(fd);
		if (socket::bind(fd, *ep)) {
			return push_neterror(L, "bind");
		}
		if (protocol == IPPROTO_TCP) {
			int backlog = (int)luaL_optinteger(L, 4, kDefaultBackLog);
			if (socket::listen(fd, backlog)) {
				return push_neterror(L, "listen");
			}
		}
		return 1;
	}
	static int select(lua_State* L) {
		return 0;
	}
}

extern "C" __declspec(dllexport)
int luaopen_bee_net(lua_State* L) {
	bee::net::socket::initialize();
	luaL_Reg lib[] = {
		{ "connect", luanet::connect },
		{ "bind", luanet::bind },
		{ "select", luanet::select },
		{ NULL, NULL }
	};
	luaL_newlib(L, lib);
	return 1;
}
