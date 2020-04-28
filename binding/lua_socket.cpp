#include <lua.hpp>
#if defined _WIN32
#include <winsock.h>
#else
#include <sys/select.h>
#endif
#include <bee/error.h>
#include <bee/lua/binding.h>
#include <bee/net/endpoint.h>
#include <bee/net/socket.h>
#include <bee/filesystem.h>
#include <chrono>
#include <limits>
#include <thread>
#include <assert.h>

namespace bee::lua_socket {
    using namespace bee::net;
    static const int kDefaultBackLog = 5;
    struct luafd {
        enum class tag {
            unknown,
            connect,
            listen,
            accept,
        };
        socket::fd_t     fd;
        socket::protocol protocol;
        tag              type;
        std::string      path;
        luafd(socket::fd_t s, socket::protocol p, tag t) : fd(s), protocol(p), type(t) {}
    };
    static int push_neterror(lua_State* L, const char* msg) {
        auto error = make_neterror(msg);
        lua_pushnil(L);
        lua_pushfstring(L, "(%d) %s", error.code().value(), error.what());
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
            luaL_error(L, "invalid protocol `%s`.", type.data());
            return socket::protocol::none;
        }
    }
    static endpoint read_endpoint(lua_State* L, socket::protocol protocol, int idx) {
        std::string_view ip = lua::to_strview(L, idx);
        if (protocol != socket::protocol::unix) {
            int port = (int)luaL_checkinteger(L, idx + 1);
            endpoint ep = endpoint::from_hostname(ip, port);
            if (!ep.valid()) {
                luaL_error(L, "invalid address: %s:%d", ip, port);
            }
            return ep;
        }
        endpoint ep = endpoint::from_unixpath(ip);
        if (!ep.valid()) {
            luaL_error(L, "invalid address: %s", ip);
        }
        return ep;
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
    static luafd& pushfd(lua_State* L, socket::fd_t fd, socket::protocol protocol, luafd::tag type);
    socket::fd_t  checksocket(lua_State* L, int idx) {
        return checkfd(L, idx).fd;
    }
    void pushsocket(lua_State* L, socket::fd_t fd) {
        // 鉴于protocol目前的作用，硬编码为tcp也没问题？
        pushfd(L, fd, socket::protocol::tcp, luafd::tag::unknown);
    }
    static int accept(lua_State* L) {
        luafd&       self = checkfd(L, 1);
        socket::fd_t newfd;
        if (socket::status::success != socket::accept(self.fd, newfd)) {
            return push_neterror(L, "accept");
        }
        pushfd(L, newfd, self.protocol, luafd::tag::accept);
        return 1;
    }
    static int recv(lua_State* L) {
        luafd&      self = checkfd(L, 1);
        lua_Integer len = luaL_optinteger(L, 2, LUAL_BUFFERSIZE);
        if (len > (std::numeric_limits<int>::max)()) {
            return luaL_argerror(L, 2, "invalid number");
        }
        luaL_Buffer b;
        luaL_buffinit(L, &b);
        char* buf = luaL_prepbuffsize(&b, (size_t)len);
        int   rc;
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
        luafd&      self = checkfd(L, 1);
        size_t      len;
        const char* buf = luaL_checklstring(L, 2, &len);
        int         rc;
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
        luafd&      self = checkfd(L, 1);
        lua_Integer len = luaL_optinteger(L, 2, LUAL_BUFFERSIZE);
        if (len > (std::numeric_limits<int>::max)()) {
            return luaL_argerror(L, 2, "invalid number");
        }
        endpoint    ep = endpoint::from_empty();
        luaL_Buffer b;
        luaL_buffinit(L, &b);
        char* buf = luaL_prepbuffsize(&b, (size_t)len);
        int   rc;
        switch (socket::recvfrom(self.fd, rc, buf, (int)len, ep)) {
        case socket::status::close:
            lua_pushnil(L);
            return 1;
        case socket::status::wait:
            lua_pushboolean(L, 0);
            return 1;
        case socket::status::success: {
            luaL_pushresultsize(&b, rc);
            auto [ip, port] = ep.info();
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
        luafd&           self = checkfd(L, 1);
        size_t           len;
        const char*      buf = luaL_checklstring(L, 2, &len);
        std::string_view ip = lua::to_strview(L, 3);
        int              port = (int)luaL_checkinteger(L, 4);
        auto             ep = endpoint::from_hostname(ip, port);
        if (!ep.valid()) {
            return luaL_error(L, "invalid address: %s:%d", ip, port);
        }
        int rc;
        switch (socket::sendto(self.fd, rc, buf, (int)len, ep)) {
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
        if (self.protocol == socket::protocol::unix && self.type == luafd::tag::listen) {
#if defined _WIN32
            if (!socket::supportUnixDomainSocket()) {
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
        else if (flag[0] == 'w') {
            return shutdown(L, self, socket::shutdown_flag::write);
        }
        lua_pushnil(L);
        lua_pushstring(L, "invalid flag");
        return 2;
    }
    static int tostring(lua_State* L) {
        luafd& self = checkfd(L, 1);
        if (self.fd == socket::retired_fd) {
            lua_pushstring(L, "socket (closed)");
            return 1;
        }
        lua_pushfstring(L, "socket (%d)", self.fd);
        return 1;
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
        int    err = socket::errcode(self.fd);
        if (err == 0) {
            lua_pushboolean(L, true);
            return 1;
        }
        auto error = make_error(err);
        lua_pushnil(L);
        lua_pushfstring(L, "(%d) %s", err, error.what());
        return 2;
    }
    static int info(lua_State* L) {
        luafd&           self = checkfd(L, 1);
        std::string_view which = lua::to_strview(L, 2);
        if (which == "peer") {
            endpoint ep = endpoint::from_empty();
            if (!socket::getpeername(self.fd, ep)) {
                return push_neterror(L, "getpeername");
            }
            auto [ip, port] = ep.info();
            lua_pushlstring(L, ip.data(), ip.size());
            lua_pushinteger(L, port);
            return 2;
        }
        else if (which == "socket") {
            endpoint ep = endpoint::from_empty();
            if (!socket::getsockname(self.fd, ep)) {
                return push_neterror(L, "getsockname");
            }
            auto [ip, port] = ep.info();
            lua_pushlstring(L, ip.data(), ip.size());
            lua_pushinteger(L, port);
            return 2;
        }
        return 0;
    }
    static luafd& pushfd(lua_State* L, socket::fd_t fd, socket::protocol protocol, luafd::tag type) {
        luafd* self = (luafd*)lua_newuserdatauv(L, sizeof(luafd), 0);
        new (self) luafd(fd, protocol, type);
        if (newObject(L, "socket")) {
            luaL_Reg mt[] = {
                {"accept", accept},
                {"recv", recv},
                {"send", send},
                {"recvfrom", recvfrom},
                {"sendto", sendto},
                {"close", close},
                {"shutdown", shutdown},
                {"status", status},
                {"info", info},
                {"__tostring", tostring},
                {"__gc", gc},
                {NULL, NULL},
            };
            luaL_setfuncs(L, mt, 0);
            lua_pushvalue(L, -1);
            lua_setfield(L, -2, "__index");
        }
        lua_setmetatable(L, -2);
        return *self;
    }
#if defined _WIN32
    static bool safe_exists(std::string& path) {
        FILE* f = _wfopen(u2w(path).c_str(), L"rb");
        if (f) {
            fclose(f);
            return true;
        }
        return errno != ENOENT;
    }
#endif
    static int connect(lua_State* L) {
        socket::protocol protocol = read_protocol(L, 1);
        auto             ep = read_endpoint(L, protocol, 2);
#if defined _WIN32
        if (ep.family() == AF_UNIX) {
            auto [path, type] = ep.info();
            if (type == 0 && !safe_exists(path)) {
                auto error = make_error(WSAECONNREFUSED, "socket");
                lua_pushnil(L);
                lua_pushfstring(L, "(%d) %s", error.code().value(), error.what());
                return 2;
            }
        }
#endif
        socket::fd_t fd = socket::open(protocol, ep);
        if (fd == socket::retired_fd) {
            return push_neterror(L, "socket");
        }
        pushfd(L, fd, protocol, luafd::tag::connect);
        switch (socket::connect(fd, ep)) {
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
        auto             ep = read_endpoint(L, protocol, 2);
        int          backlog = read_backlog(L, protocol);
        socket::fd_t fd = socket::open(protocol, ep);
        if (fd == socket::retired_fd) {
            return push_neterror(L, "socket");
        }
#if defined _WIN32
        luafd& self =
#endif
            pushfd(L, fd, protocol, luafd::tag::listen);
        if (ep.family() == AF_UNIX) {
            socket::unlink(ep);
#if defined _WIN32
            if (!socket::supportUnixDomainSocket()) {
                auto [path, type] = ep.info();
                if (type == 0) {
                    self.path = path;
                }
            }
#endif
        }
        if (socket::status::success != socket::bind(fd, ep)) {
            return push_neterror(L, "bind");
        }
        if (protocol != socket::protocol::udp) {
            if (socket::status::success != socket::listen(fd, backlog)) {
                return push_neterror(L, "listen");
            }
        }
        return 1;
    }
    static int pair(lua_State* L) {
        socket::fd_t sv[2];
        if (!socket::pair(sv)) {
            return push_neterror(L, "socketpair");
        }
        pushfd(L, sv[0], socket::protocol::unix, luafd::tag::accept);
        pushfd(L, sv[1], socket::protocol::unix, luafd::tag::connect);
        return 2;
    }
#if defined(_WIN32)
#define MAXFD_INIT()
#define MAXFD_SET(fd)
#define MAXFD_GET() 0
#define EXFDS_INIT() fd_set exceptfds
#define EXFDS_ZERO() FD_ZERO(&exceptfds)
#define EXFDS_SET(connect, fd) \
    if (connect)               \
    FD_SET((fd), &exceptfds)
#define EXFDS_GET() (&exceptfds)
#define EXFDS_UPDATE(wfds)                         \
    for (u_int i = 0; i < exceptfds.fd_count; ++i) \
    FD_SET(exceptfds.fd_array[i], &(wfds))
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
        bool   read_finish = lua_type(L, 1) != LUA_TTABLE;
        bool   write_finish = lua_type(L, 2) != LUA_TTABLE;
        int    rmax = read_finish ? 0 : (int)luaL_len(L, 1);
        int    wmax = write_finish ? 0 : (int)luaL_len(L, 2);
        double timeo = luaL_optnumber(L, 3, -1);
        if (!rmax && !wmax) {
            if (timeo == -1) {
                return luaL_error(L, "no open sockets to check and no timeout set");
            }
            else {
                std::this_thread::sleep_for(std::chrono::duration<double>(timeo));
                lua_newtable(L);
                lua_newtable(L);
                return 2;
            }
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
        fd_set      readfds, writefds;
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
                EXFDS_SET(self.type == luafd::tag::connect, self.fd);
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
#if defined _WIN32
    static int simulationUDS(lua_State* L) {
        bool open = !!lua_toboolean(L, 1);
        socket::simulationUnixDomainSocket(open);
        return 0;
    }
#endif
    static int luaopen(lua_State* L) {
        socket::initialize();
        luaL_Reg lib[] = {
            {"connect", connect},
            {"bind", bind},
            {"pair", pair},
            {"select", select},
#if defined _WIN32
            {"simulationUDS", simulationUDS},
#endif
            {NULL, NULL}};
        lua_newtable(L);
        luaL_setfuncs(L, lib, 0);
        return 1;
    }
}

DEFINE_LUAOPEN(socket)
