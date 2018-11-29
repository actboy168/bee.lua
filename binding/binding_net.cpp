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
	static socket::fd_t checkfd(lua_State* L, int idx) {
		luaL_checktype(L, idx, LUA_TLIGHTUSERDATA);
		return (socket::fd_t)lua_touserdata(L, idx);
	}
	static void constructor(lua_State* L, socket::fd_t fd) {
		lua_pushlightuserdata(L, (void*)fd);
		socket::reuse(fd);
		socket::nonblocking(fd);
	}
	static int accept(lua_State* L) {
		luaL_checktype(L, 1, LUA_TLIGHTUSERDATA);
		socket::fd_t fd = (socket::fd_t)lua_touserdata(L, 1);
		socket::fd_t newfd;
		if (socket::accept(fd, newfd)) {
			return push_neterror(L, "accept");
		}
		constructor(L, newfd);
		return 1;
	}
	static int recv(lua_State* L) {
		luaL_checktype(L, 1, LUA_TLIGHTUSERDATA);
		socket::fd_t fd = (socket::fd_t)lua_touserdata(L, 1);
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
	static int send(lua_State* L) {
		luaL_checktype(L, 1, LUA_TLIGHTUSERDATA);
		socket::fd_t fd = (socket::fd_t)lua_touserdata(L, 1);
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
	static int recvfrom(lua_State* L) {
		luaL_checktype(L, 1, LUA_TLIGHTUSERDATA);
		socket::fd_t fd = (socket::fd_t)lua_touserdata(L, 1);
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
			auto[ip, port] = ep.info();
			lua_pushlstring(L, ip.data(), ip.size());
			lua_pushinteger(L, port);
			return 3;
		}
		}
	}
	static int sendto(lua_State* L) {
		luaL_checktype(L, 1, LUA_TLIGHTUSERDATA);
		socket::fd_t fd = (socket::fd_t)lua_touserdata(L, 1);
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
		luaL_checktype(L, 1, LUA_TLIGHTUSERDATA);
		socket::fd_t fd = (socket::fd_t)lua_touserdata(L, 1);
		if (!socket::close(fd)) {
			return push_neterror(L, "close");
		}
		lua_pushboolean(L, 1);
		return 1;
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
		constructor(L, fd);
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
		constructor(L, fd);
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
		luaL_checktype(L, 1, LUA_TTABLE);
		luaL_checktype(L, 2, LUA_TTABLE);
		int rmax = (int)luaL_len(L, 1);
		int wmax = (int)luaL_len(L, 2);
		double timeo = luaL_optnumber(L, 3, -1);
		if (!rmax && !wmax && timeo == -1) {
			return luaL_error(L, "no open sockets to check and no timeout set");
		}
		struct timeval timeout, *timeop = &timeout;
		if (timeo < 0) {
			timeop = NULL;
			if (rmax > FD_SETSIZE || wmax > FD_SETSIZE) {
				return luaL_error(L, "sockets too much");
			}
		}
		else {
			int ntime = 1 + (std::max)(rmax, wmax) / FD_SETSIZE;
			timeo /= ntime;
			timeout.tv_sec = (long)timeo;
			timeout.tv_usec = (long)((timeo - timeout.tv_sec) * 1000000);
		}
		lua_settop(L, 3);
		lua_newtable(L);
		lua_newtable(L);
		lua_Integer rout = 0, wout = 0;
		fd_set readfd, writefd;
		bool read_finish = false, write_finish = false;
		for (int x = 0; !read_finish && !write_finish; x += FD_SETSIZE) {
			FD_ZERO(&readfd);
			for (int r = 0; !read_finish && r < FD_SETSIZE; ++r) {
				if (LUA_TNIL == lua_rawgeti(L, 1, x + r)) {
					read_finish = true;
					lua_pop(L, 1);
					break;
				}
				FD_SET(checkfd(L, -1), &readfd);
				lua_pop(L, 1);
			}
			FD_ZERO(&writefd);
			for (int w = 0; !write_finish && w < FD_SETSIZE; ++w) {
				if (LUA_TNIL == lua_rawgeti(L, 2, x + w)) {
					write_finish = true;
					lua_pop(L, 1);
					break;
				}
				FD_SET(checkfd(L, -1), &writefd);
				lua_pop(L, 1);
			}
			int ok = ::select(0, &readfd, &writefd, NULL, timeop);
			if (ok > 0) {
				for (u_int i = 0; i < readfd.fd_count; ++i) {
					lua_pushlightuserdata(L, (void*)readfd.fd_array[i]);
					lua_rawseti(L, 4, ++rout);
				}
				for (u_int i = 0; i < writefd.fd_count; ++i) {
					lua_pushlightuserdata(L, (void*)writefd.fd_array[i]);
					lua_rawseti(L, 5, ++wout);
				}
			}
			else if (ok < 0) {
				return push_neterror(L, "select");
			}
		}
		return 2;
	}
}

extern "C" __declspec(dllexport)
int luaopen_bee_net(lua_State* L) {
	bee::net::socket::initialize();
	luaL_Reg lib[] = {
		{ "accept", luanet::accept },
		{ "recv", luanet::recv },
		{ "send", luanet::send },
		{ "recvfrom", luanet::recvfrom },
		{ "sendto", luanet::sendto },
		{ "close", luanet::close },
		{ "connect", luanet::connect },
		{ "bind", luanet::bind },
		{ "select", luanet::select },
		{ NULL, NULL }
	};
	luaL_newlib(L, lib);
	return 1;
}
