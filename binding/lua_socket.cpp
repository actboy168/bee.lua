#include <lua.hpp>
#if defined _WIN32
#include <winsock.h>
#endif
#include <bee/lua/binding.h>
#include <bee/net/socket.h>
#include <bee/net/endpoint.h>
#include <bee/error.h>
#include <limits>
#if defined _WIN32
#include <bee/net/unixsocket.h>
#endif

namespace bee::lua_socket {
    using namespace bee::net;
    static const int kDefaultBackLog = 5;
    struct luafd {
        socket::fd_t     fd;
        socket::protocol protocol;
        bool connect = false;
		std::string path;
        luafd(socket::fd_t s, socket::protocol p) : fd(s) , protocol(p)
        { }
    };
    static int push_neterror(lua_State* L, const char* msg) {
        auto error = make_neterror(msg);
        lua_pushnil(L);
        lua_pushfstring(L, "%s (%d)", error.what(), error.code().value());
        return 2;
    }
    static socket::protocol read_protocol(lua_State* L, int idx) {
        std::string_view type = lua::to_strview(L, idx);
        if (type == "tcp") {
            return socket::protocol::tcp;
        }
        else if (type == "udp") {
            return socket::protocol::udp;
        }
        else if (type == "unix") {
            return socket::protocol::unix;
        }
        else {
            luaL_error(L, "invalid protocol `%s`.", type);
            return socket::protocol::none;
        }
    }
    static auto read_endpoint(lua_State* L, socket::protocol protocol, int idx) {
        std::string_view ip = lua::to_strview(L, idx);
        if (protocol != socket::protocol::unix) {
            int port = (int)luaL_checkinteger(L, idx + 1);
            return endpoint::from_hostname(ip, port);
        }
        return endpoint::from_unixpath(ip);
    }
    static int read_backlog(lua_State* L, socket::protocol protocol) {
        switch (protocol) {
        case socket::protocol::tcp:
            return (int)luaL_optinteger(L, 4, kDefaultBackLog);
        case socket::protocol::unix:
            return (int)luaL_optinteger(L, 3, kDefaultBackLog);
        default:
        case socket::protocol::udp:
            return kDefaultBackLog;
        }
    }
    static luafd& tofd(lua_State* L, int idx) {
        return *(luafd*)lua_touserdata(L, idx);
    }
    static luafd& checkfd(lua_State* L, int idx) {
        return *(luafd*)getObject(L, idx, "socket");
    }
    static luafd& pushfd(lua_State* L, socket::fd_t fd, socket::protocol protocol);
    static luafd& constructor(lua_State* L, socket::fd_t fd, socket::protocol protocol) {
        luafd& self = pushfd(L, fd, protocol);
        socket::nonblocking(fd);
        if (protocol != socket::protocol::unix) {
            socket::reuse(fd);
        }
        return self;
    }
    static int accept(lua_State* L) {
        luafd& self = checkfd(L, 1);
        socket::fd_t newfd;
        if (socket::status::success != socket::accept(self.fd, newfd)) {
            return push_neterror(L, "accept");
        }
        constructor(L, newfd, self.protocol);
        return 1;
    }
    static int recv(lua_State* L) {
        luafd& self = checkfd(L, 1);
        lua_Integer len = luaL_optinteger(L, 2, LUAL_BUFFERSIZE);
        if (len > (std::numeric_limits<int>::max)()) {
            return luaL_error(L, "bad argument #1 to 'recv' (invalid number)");
        }
        luaL_Buffer b;
        luaL_buffinit(L, &b);
        char* buf = luaL_prepbuffsize(&b, (size_t)len);
        int rc;
        switch (socket::recv(self.fd, rc, buf, (int)len)) {
        case socket::status::close:
            lua_pushnil(L);
            return 1;
        case socket::status::wait:
            lua_pushboolean(L, 0);
            return 1;
        case socket::status::success:
            luaL_pushresultsize(&b, rc);
            return 1;
        default:
            assert(false);
            [[fallthrough]];
        case socket::status::failed:
            return push_neterror(L, "recv");
        }
    }
    static int send(lua_State* L) {
        luafd& self = checkfd(L, 1);
        size_t len;
        const char* buf = luaL_checklstring(L, 2, &len);
        int rc;
        switch (socket::send(self.fd, rc, buf, (int)len)) {
        case socket::status::wait:
            lua_pushboolean(L, 0);
            return 1;
        case socket::status::success:
            lua_pushinteger(L, rc);
            return 1;
        default:
            assert(false);
            [[fallthrough]];
        case socket::status::failed:
            return push_neterror(L, "send");
        }
    }
    static int recvfrom(lua_State* L) {
        luafd& self = checkfd(L, 1);
        lua_Integer len = luaL_optinteger(L, 2, LUAL_BUFFERSIZE);
        if (len > (std::numeric_limits<int>::max)()) {
            return luaL_error(L, "bad argument #1 to 'recv' (invalid number)");
        }
        endpoint ep = endpoint::from_empty();
        luaL_Buffer b;
        luaL_buffinit(L, &b);
        char* buf = luaL_prepbuffsize(&b, (size_t)len);
        int rc;
        switch (socket::recvfrom(self.fd, rc, buf, (int)len, ep)) {
        case socket::status::close:
            lua_pushnil(L);
            return 1;
        case socket::status::wait:
            lua_pushboolean(L, 0);
            return 1;
        case socket::status::success: {
            luaL_pushresultsize(&b, rc);
            auto[ip, port] = ep.info();
            lua_pushlstring(L, ip.data(), ip.size());
            lua_pushinteger(L, port);
            return 3;
        }
        default:
            assert(false);
            [[fallthrough]];
        case socket::status::failed:
            return push_neterror(L, "recvfrom");
        }
    }
    static int sendto(lua_State* L) {
        luafd& self = checkfd(L, 1);
        size_t len;
        const char* buf = luaL_checklstring(L, 2, &len);
        std::string_view ip = lua::to_strview(L, 3);
        int port = (int)luaL_checkinteger(L, 4);
        auto ep = endpoint::from_hostname(ip, port);
        if (!ep) {
            lua_pushlstring(L, ep.error().data(), ep.error().size());
            return lua_error(L);
        }
        int rc;
        switch (socket::sendto(self.fd, rc, buf, (int)len, ep.value())) {
        case socket::status::wait:
            lua_pushboolean(L, 0);
            return 1;
        case socket::status::success:
            lua_pushinteger(L, rc);
            return 1;
        default:
            assert(false);
            [[fallthrough]];
        case socket::status::failed:
            return push_neterror(L, "sendto");
        }
    }
    static int close(lua_State* L) {
        luafd& self = checkfd(L, 1);
        if (self.fd == socket::retired_fd) {
            lua_pushboolean(L, 1);
            return 1;
        }
        socket::fd_t fd = self.fd;
        self.fd = socket::retired_fd;
        if (self.protocol == socket::protocol::unix) {
#if defined _WIN32
			if (socket::u_enable()) {
				::DeleteFileW(u2w(self.path).c_str());
			}
			else {
				socket::unlink(fd);
			}
#else
			socket::unlink(fd);
#endif
        }
        if (!socket::close(fd)) {
            return push_neterror(L, "close");
        }
        lua_pushboolean(L, 1);
        return 1;
    }
    static int shutdown(lua_State* L, luafd& self, socket::shutdown_flag flag) {
        if (!socket::shutdown(self.fd, flag)) {
            return push_neterror(L, "shutdown");
        }
        lua_pushboolean(L, true);
        return 1;
    }
    static int shutdown(lua_State* L) {
        luafd& self = checkfd(L, 1);
        if (lua_isnoneornil(L, 2)) {
            return shutdown(L, self, socket::shutdown_flag::both);
        }
        std::string_view flag = lua::to_strview(L, 2);
        if (flag[0] == 'r') {
            return shutdown(L, self, socket::shutdown_flag::read);
        }
        else if(flag[0] == 'w') {
            return shutdown(L, self, socket::shutdown_flag::write);
        }
        lua_pushnil(L);
        lua_pushstring(L, "invalid flag");
        return 2;
    }
    static int gc(lua_State* L) {
        luafd& self = checkfd(L, 1);
        if (self.fd == socket::retired_fd) {
            return 0;
        }
        socket::fd_t fd = self.fd;
        self.fd = socket::retired_fd;
        if (self.protocol == socket::protocol::unix) {
            socket::unlink(fd);
        }
        socket::close(fd);
        return 0;
    }
    static int status(lua_State* L) {
        luafd& self = checkfd(L, 1);
        int err = socket::errcode(self.fd);
        if (err == 0) {
            lua_pushboolean(L, true);
            return 1;
        }
        auto error = make_error(err);
        lua_pushnil(L);
        lua_pushfstring(L, "%s (%d)", err, error.what());
        return 2;
    }
    static int info(lua_State* L) {
        luafd& self = checkfd(L, 1);
        std::string_view which = lua::to_strview(L, 2);
        if (which == "peer") {
            endpoint ep = endpoint::from_empty();
            if (!socket::getpeername(self.fd, ep)) {
                return push_neterror(L, "getpeername");
            }
            auto[ip, port] = ep.info();
            lua_pushlstring(L, ip.data(), ip.size());
            lua_pushinteger(L, port);
            return 2;
        }
        else if (which == "socket") {
            endpoint ep = endpoint::from_empty();
            if (!socket::getsockname(self.fd, ep)) {
                return push_neterror(L, "getsockname");
            }
            auto[ip, port] = ep.info();
            lua_pushlstring(L, ip.data(), ip.size());
            lua_pushinteger(L, port);
            return 2;
        }
        return 0;
    }
    static luafd& pushfd(lua_State* L, socket::fd_t fd, socket::protocol protocol) {
        luafd* self = (luafd*)lua_newuserdata(L, sizeof(luafd));
        new (self) luafd(fd, protocol);
        if (newObject(L, "socket")) {
            luaL_Reg mt[] = {
                { "accept",   accept },
                { "recv",     recv },
                { "send",     send },
                { "recvfrom", recvfrom },
                { "sendto",   sendto },
                { "close",    close },
                { "shutdown", shutdown },
                { "status",   status },
                { "info",     info },
                { "__gc",     gc },
                { NULL, NULL },
            };
            luaL_setfuncs(L, mt, 0);
            lua_pushvalue(L, -1);
            lua_setfield(L, -2, "__index");
        }
        lua_setmetatable(L, -2);
        return *self;
    }
    static int connect(lua_State* L) {
        socket::protocol protocol = read_protocol(L, 1);
        auto ep = read_endpoint(L, protocol, 2);
        if (!ep) {
            lua_pushlstring(L, ep.error().data(), ep.error().size());
            return lua_error(L);
        }
        socket::fd_t fd = socket::open(ep->family(), protocol);
        if (fd == socket::retired_fd) {
            return push_neterror(L, "socket");
        }
        luafd& self = constructor(L, fd, protocol);
        self.connect = true;
        switch (socket::connect(fd, *ep)) {
        case socket::status::success:
            lua_pushboolean(L, true);
            return 2;
        case socket::status::wait:
            lua_pushboolean(L, false);
            return 2;
        default:
            assert(false);
            [[fallthrough]];
        case socket::status::failed:
            return push_neterror(L, "connect");
        }
    }
    static int bind(lua_State* L) {
        socket::protocol protocol = read_protocol(L, 1);
        auto ep = read_endpoint(L, protocol, 2);
        if (!ep) {
            lua_pushnil(L);
            lua_pushlstring(L, ep.error().data(), ep.error().size());
            return 2;
        }
        int backlog = read_backlog(L, protocol);
        socket::fd_t fd = socket::open(ep->family(), protocol);
        if (fd == socket::retired_fd) {
            return push_neterror(L, "socket");
        }
#if defined _WIN32
        luafd& self =
#endif
        constructor(L, fd, protocol);
        if (socket::status::success != socket::bind(fd, *ep)) {
            return push_neterror(L, "bind");
        }
#if defined _WIN32
		if (socket::u_enable()) {
			auto[path, port] = ep->info();
			self.path = path;
		}
#endif
        if (protocol != socket::protocol::udp) {
            if (socket::status::success != socket::listen(fd, backlog)) {
                return push_neterror(L, "listen");
            }
        }
        return 1;
    }
#if defined(_WIN32)
#define MAXFD_INIT()
#define MAXFD_SET(fd)
#define MAXFD_GET() 0
#define EXFDS_INIT() fd_set exceptfds
#define EXFDS_ZERO() FD_ZERO(&exceptfds)
#define EXFDS_SET(connect, fd) if (connect) FD_SET((fd), &exceptfds) 
#define EXFDS_GET() (&exceptfds)
#define EXFDS_UPDATE(wfds) for (u_int i = 0; i < exceptfds.fd_count; ++i) FD_SET(exceptfds.fd_array[i], &(wfds))
#else
#define MAXFD_INIT() int maxfd = 0
#define MAXFD_SET(fd) maxfd = (std::max)(maxfd, (int)(fd))
#define MAXFD_GET() (maxfd + 1)
#define EXFDS_INIT()
#define EXFDS_ZERO()
#define EXFDS_SET(connect, fd)
#define EXFDS_GET() NULL
#define EXFDS_UPDATE(wfds)
#endif
    static int select(lua_State* L) {
        bool read_finish = lua_type(L, 1) != LUA_TTABLE;
        bool write_finish = lua_type(L, 2) != LUA_TTABLE;
        int rmax = read_finish ? 0 : (int)luaL_len(L, 1);
        int wmax = write_finish ? 0 : (int)luaL_len(L, 2);
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
        fd_set readfds, writefds;
        EXFDS_INIT();
        for (int x = 1; !read_finish || !write_finish; x += FD_SETSIZE) {
            MAXFD_INIT();
            FD_ZERO(&readfds);
            FD_ZERO(&writefds);
            EXFDS_ZERO();
            int r = 0, w = 0;
            for (; !read_finish && r < FD_SETSIZE; ++r) {
                if (LUA_TNIL == lua_rawgeti(L, 1, x + r)) {
                    read_finish = true;
                    lua_pop(L, 1);
                    break;
                }

                socket::fd_t fd = checkfd(L, -1).fd;
                MAXFD_SET(fd);
                FD_SET(fd, &readfds);
                lua_pop(L, 1);
            }
            for (; !write_finish && w < FD_SETSIZE; ++w) {
                if (LUA_TNIL == lua_rawgeti(L, 2, x + w)) {
                    write_finish = true;
                    lua_pop(L, 1);
                    break;
                }
                luafd& self = checkfd(L, -1);
                MAXFD_SET(self.fd);
                FD_SET(self.fd, &writefds);
                EXFDS_SET(self.connect, self.fd);
                lua_pop(L, 1);
            }
            int ok = ::select(MAXFD_GET(), &readfds, &writefds, EXFDS_GET(), timeop);
            if (ok > 0) {
                EXFDS_UPDATE(writefds);
                for (int i = 0; i < r; ++i) {
                    if (LUA_TUSERDATA == lua_rawgeti(L, 1, x + i) && FD_ISSET(tofd(L, -1).fd, &readfds)) {
                        lua_rawseti(L, 4, ++rout);
                    }
                    else {
                        lua_pop(L, 1);
                    }
                }
                for (int i = 0; i < w; ++i) {
                    if (LUA_TUSERDATA == lua_rawgeti(L, 2, x + i) && FD_ISSET(tofd(L, -1).fd, &writefds)) {
                        lua_rawseti(L, 5, ++wout);
                    }
                    else {
                        lua_pop(L, 1);
                    }
                }
            }
            else if (ok < 0) {
                return push_neterror(L, "select");
            }
        }
        return 2;
    }

    int luaopen(lua_State* L) {
        socket::initialize();
        luaL_Reg lib[] = {
            { "connect",  connect },
            { "bind",     bind },
            { "select",   select },
            { NULL, NULL }
        };
        luaL_newlib(L, lib);
        return 1;
    }
}

DEFINE_LUAOPEN(socket)
