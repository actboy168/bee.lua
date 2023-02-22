#include <binding/binding.h>
#if defined _WIN32
#include <winsock.h>
#else
#include <sys/select.h>
#endif
#include <bee/error.h>
#include <bee/net/endpoint.h>
#include <bee/net/socket.h>
#include <bee/nonstd/unreachable.h>
#include <bee/thread/simplethread.h>
#include <limits>

namespace bee::lua_socket {
    using namespace bee::net;
    static const int kDefaultBackLog = 5;
    static int push_neterror(lua_State* L, const char* msg) {
        auto error = make_neterror(msg);
        lua_pushnil(L);
        lua::push_errormesg(L, msg, error);
        return 2;
    }
    static endpoint read_endpoint(lua_State* L, int idx) {
        std::string_view ip = lua::to_strview(L, idx);
        if (lua_isnoneornil(L, idx + 1)) {
            endpoint ep = endpoint::from_unixpath(ip);
            if (!ep.valid()) {
                luaL_error(L, "invalid address: %s", ip.data());
            }
            return ep;
        }
        else {
            int port = (int)luaL_checkinteger(L, idx + 1);
            endpoint ep = endpoint::from_hostname(ip, port);
            if (!ep.valid()) {
                luaL_error(L, "invalid address: %s:%d", ip.data(), port);
            }
            return ep;
        }
    }
    static socket::fd_t& tofd(lua_State* L, int idx) {
        return *(socket::fd_t*)lua_touserdata(L, idx);
    }
    static socket::fd_t& checkfd(lua_State* L, int idx) {
        return *(socket::fd_t*)getObject(L, idx, "socket");
    }
    static socket::fd_t& pushfd(lua_State* L, socket::fd_t fd);
    static int accept(lua_State* L) {
        socket::fd_t fd = checkfd(L, 1);
        socket::fd_t newfd;
        if (socket::status::success != socket::accept(fd, newfd)) {
            return push_neterror(L, "accept");
        }
        pushfd(L, newfd);
        return 1;
    }
    static int recv(lua_State* L) {
        socket::fd_t fd  = checkfd(L, 1);
        lua_Integer  len = luaL_optinteger(L, 2, LUAL_BUFFERSIZE);
        if (len > (std::numeric_limits<int>::max)()) {
            return luaL_argerror(L, 2, "invalid number");
        }
        luaL_Buffer b;
        luaL_buffinit(L, &b);
        char* buf = luaL_prepbuffsize(&b, (size_t)len);
        int   rc;
        switch (socket::recv(fd, rc, buf, (int)len)) {
        case socket::status::close:
            lua_pushnil(L);
            return 1;
        case socket::status::wait:
            lua_pushboolean(L, 0);
            return 1;
        case socket::status::success:
            luaL_pushresultsize(&b, rc);
            return 1;
        case socket::status::failed:
            return push_neterror(L, "recv");
        default:
            std::unreachable();
        }
    }
    static int send(lua_State* L) {
        socket::fd_t     fd  = checkfd(L, 1);
        std::string_view buf = lua::to_strview(L, 2);
        int         rc;
        switch (socket::send(fd, rc, (const char*)buf.data(), (int)buf.size())) {
        case socket::status::wait:
            lua_pushboolean(L, 0);
            return 1;
        case socket::status::success:
            lua_pushinteger(L, rc);
            return 1;
        case socket::status::failed:
            return push_neterror(L, "send");
        default:
            std::unreachable();
        }
    }
    static int recvfrom(lua_State* L) {
        socket::fd_t fd  = checkfd(L, 1);
        lua_Integer  len = luaL_optinteger(L, 2, LUAL_BUFFERSIZE);
        if (len > (std::numeric_limits<int>::max)()) {
            return luaL_argerror(L, 2, "invalid number");
        }
        endpoint    ep = endpoint::from_empty();
        luaL_Buffer b;
        luaL_buffinit(L, &b);
        char* buf = luaL_prepbuffsize(&b, (size_t)len);
        int   rc;
        switch (socket::recvfrom(fd, rc, buf, (int)len, ep)) {
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
        case socket::status::failed:
            return push_neterror(L, "recvfrom");
        default:
            std::unreachable();
        }
    }
    static int sendto(lua_State* L) {
        socket::fd_t     fd = checkfd(L, 1);
        std::string_view buf = lua::to_strview(L, 2);
        std::string_view ip = lua::to_strview(L, 3);
        int              port = (int)luaL_checkinteger(L, 4);
        auto             ep = endpoint::from_hostname(ip, port);
        if (!ep.valid()) {
            return luaL_error(L, "invalid address: %s:%d", ip, port);
        }
        int rc;
        switch (socket::sendto(fd, rc, buf.data(), (int)buf.size(), ep)) {
        case socket::status::wait:
            lua_pushboolean(L, 0);
            return 1;
        case socket::status::success:
            lua_pushinteger(L, rc);
            return 1;
        case socket::status::failed:
            return push_neterror(L, "sendto");
        default:
            std::unreachable();
        }
    }
    static bool socket_destroy(socket::fd_t& fd) {
        if (fd == socket::retired_fd) {
            return true;
        }
        bool ok = socket::close(fd);
        fd = socket::retired_fd;
        return ok;
    }
    static int close(lua_State* L) {
        socket::fd_t& fd = checkfd(L, 1);
        if (!socket_destroy(fd)) {
            return push_neterror(L, "close");
        }
        lua_pushboolean(L, 1);
        return 1;
    }
    static int shutdown(lua_State* L, socket::fd_t fd, socket::shutdown_flag flag) {
        if (!socket::shutdown(fd, flag)) {
            return push_neterror(L, "shutdown");
        }
        lua_pushboolean(L, 1);
        return 1;
    }
    static int shutdown(lua_State* L) {
        socket::fd_t fd = checkfd(L, 1);
        if (lua_isnoneornil(L, 2)) {
            return shutdown(L, fd, socket::shutdown_flag::both);
        }
        std::string_view flag = lua::to_strview(L, 2);
        if (flag[0] == 'r') {
            return shutdown(L, fd, socket::shutdown_flag::read);
        }
        else if (flag[0] == 'w') {
            return shutdown(L, fd, socket::shutdown_flag::write);
        }
        lua_pushnil(L);
        lua_pushstring(L, "invalid flag");
        return 2;
    }
    static int tostring(lua_State* L) {
        socket::fd_t fd = checkfd(L, 1);
        if (fd == socket::retired_fd) {
            lua_pushstring(L, "socket (closed)");
            return 1;
        }
        lua_pushfstring(L, "socket (%d)", fd);
        return 1;
    }
    static int toclose(lua_State* L) {
        socket::fd_t& fd = checkfd(L, 1);
        socket_destroy(fd);
        return 0;
    }
    static int gc(lua_State* L) {
        socket::fd_t& fd = checkfd(L, 1);
        socket_destroy(fd);
        return 0;
    }
    static int status(lua_State* L) {
        socket::fd_t fd = checkfd(L, 1);
        int    err = socket::errcode(fd);
        if (err == 0) {
            lua_pushboolean(L, 1);
            return 1;
        }
        auto error = make_error(err);
        lua_pushnil(L);
        lua::push_errormesg(L, "status", error);
        return 2;
    }
    static int info(lua_State* L) {
        socket::fd_t     fd = checkfd(L, 1);
        std::string_view which = lua::to_strview(L, 2);
        if (which == "peer") {
            endpoint ep = endpoint::from_empty();
            if (!socket::getpeername(fd, ep)) {
                return push_neterror(L, "getpeername");
            }
            auto [ip, port] = ep.info();
            lua_pushlstring(L, ip.data(), ip.size());
            lua_pushinteger(L, port);
            return 2;
        }
        else if (which == "socket") {
            endpoint ep = endpoint::from_empty();
            if (!socket::getsockname(fd, ep)) {
                return push_neterror(L, "getsockname");
            }
            auto [ip, port] = ep.info();
            lua_pushlstring(L, ip.data(), ip.size());
            lua_pushinteger(L, port);
            return 2;
        }
        return 0;
    }
    static int handle(lua_State* L) {
        socket::fd_t fd = checkfd(L, 1);
        lua_pushlightuserdata(L, (void*)(intptr_t)fd);
        return 1;
    }
    static int option(lua_State* L) {
        socket::fd_t fd = checkfd(L, 1);
        static const char *const opts[] = {"reuseaddr", "sndbuf", "rcvbuf", NULL};
        socket::option opt = (socket::option)luaL_checkoption(L, 2, NULL, opts);
        bool ok = socket::setoption(fd, opt, (int)luaL_checkinteger(L, 3));
        if (!ok) {
            return push_neterror(L, "setsockopt");
        }
        lua_pushboolean(L, 1);
        return 1;
    }
    static int connect(lua_State* L) {
        socket::fd_t fd = checkfd(L, 1);
        auto ep = read_endpoint(L, 2);
        switch (socket::connect(fd, ep)) {
        case socket::status::success:
            lua_pushboolean(L, 1);
            return 1;
        case socket::status::wait:
            lua_pushboolean(L, 0);
            return 1;
        case socket::status::failed:
            return push_neterror(L, "connect");
        default:
            std::unreachable();
        }
    }
    static int bind(lua_State* L) {
        socket::fd_t fd = checkfd(L, 1);
        auto ep = read_endpoint(L, 2);
        socket::unlink(ep);
        if (socket::status::success != socket::bind(fd, ep)) {
            return push_neterror(L, "bind");
        }
        lua_pushboolean(L, 1);
        return 1;
    }
    static int listen(lua_State* L) {
        socket::fd_t fd = checkfd(L, 1);
        int backlog = (int)luaL_optinteger(L, 2, kDefaultBackLog);
        if (socket::status::success != socket::listen(fd, backlog)) {
            return push_neterror(L, "listen");
        }
        lua_pushboolean(L, 1);
        return 1;
    }
    static socket::fd_t& pushfd(lua_State* L, socket::fd_t fd) {
        socket::fd_t* pfd = (socket::fd_t*)lua_newuserdatauv(L, sizeof(socket::fd_t), 0);
        *pfd = fd;
        if (newObject(L, "socket")) {
            luaL_Reg mt[] = {
                {"connect", connect},
                {"bind", bind},
                {"listen", listen},
                {"accept", accept},
                {"recv", recv},
                {"send", send},
                {"recvfrom", recvfrom},
                {"sendto", sendto},
                {"close", close},
                {"shutdown", shutdown},
                {"status", status},
                {"info", info},
                {"handle", handle},
                {"option", option},
                {"__tostring", tostring},
                {"__close", toclose},
                {"__gc", gc},
                {NULL, NULL},
            };
            luaL_setfuncs(L, mt, 0);
            lua_pushvalue(L, -1);
            lua_setfield(L, -2, "__index");
        }
        lua_setmetatable(L, -2);
        return *pfd;
    }
    static int pair(lua_State* L) {
        socket::fd_t sv[2];
        if (!socket::pair(sv)) {
            return push_neterror(L, "socketpair");
        }
        pushfd(L, sv[0]);
        pushfd(L, sv[1]);
        return 2;
    }
    static int dump(lua_State* L) {
        socket::fd_t& fd = checkfd(L, 1);
        lua_pushlstring(L, (const char*)&fd, sizeof(fd));
        fd = socket::retired_fd;
        return 1;
    }
    static int undump(lua_State* L) {
        size_t sz = 0;
        const char* s = luaL_checklstring(L, 1, &sz);
        if (sz != sizeof(socket::fd_t)) {
            return luaL_error(L, "invalid string length");
        }
        pushfd(L, *(socket::fd_t*)s);
        return 1;
    }
    static int fd(lua_State* L) {
        luaL_checktype(L, 1, LUA_TLIGHTUSERDATA);
        socket::fd_t fd = (socket::fd_t)(intptr_t)lua_touserdata(L, 1);
        pushfd(L, fd);
        return 1;
    }
    static int __call(lua_State* L) {
        static const char *const opts[] = {
            "tcp", "udp", "unix", "tcp6", "udp6",
            NULL
        };
        socket::protocol protocol = (socket::protocol)luaL_checkoption(L, 2, NULL, opts);
        socket::fd_t fd = socket::open(protocol);
        if (fd == socket::retired_fd) {
            return push_neterror(L, "socket");
        }
        pushfd(L, fd);
        return 1;
    }
#if defined(_WIN32)
#define MAXFD_INIT()
#define MAXFD_SET(fd)
#define MAXFD_GET() 0
#define EXFDS_GET() (&writefds)
#else
#define MAXFD_INIT() int maxfd = 0
#define MAXFD_SET(fd) maxfd = (std::max)(maxfd, (int)(fd))
#define MAXFD_GET() (maxfd + 1)
#define EXFDS_GET() NULL
#endif
    static int select(lua_State* L) {
        bool read_finish = true, write_finish = true;
        if (lua_isnoneornil(L, 1))
            read_finish = true;
        else if (lua_type(L, 1) == LUA_TTABLE)
            read_finish = false;
        else
            luaL_typeerror(L, 1, lua_typename(L, LUA_TTABLE));
        if (lua_isnoneornil(L, 2))
            write_finish = true;
        else if (lua_type(L, 2) == LUA_TTABLE)
            write_finish = false;
        else
            luaL_typeerror(L, 2, lua_typename(L, LUA_TTABLE));

        int    rmax = read_finish ? 0 : (int)luaL_len(L, 1);
        int    wmax = write_finish ? 0 : (int)luaL_len(L, 2);
        double timeo = luaL_optnumber(L, 3, -1);
        if (!rmax && !wmax) {
            if (timeo == -1) {
                return luaL_error(L, "no open sockets to check and no timeout set");
            }
            else {
                thread_sleep(static_cast<int>(timeo * 1000));
                lua_newtable(L);
                lua_newtable(L);
                return 2;
            }
        }
        if (rmax > FD_SETSIZE || wmax > FD_SETSIZE) {
            return luaL_error(L, "sockets too much");
        }
        struct timeval timeout, *timeop = &timeout;
        if (timeo < 0) {
            timeop = NULL;
        }
        else {
            timeout.tv_sec = (long)timeo;
            timeout.tv_usec = (long)((timeo - timeout.tv_sec) * 1000000);
        }
        lua_settop(L, 3);
        lua_newtable(L);
        lua_newtable(L);
        lua_Integer rout = 0, wout = 0;
        fd_set      readfds, writefds;
        for (int x = 1; !read_finish || !write_finish; x += FD_SETSIZE) {
            MAXFD_INIT();
            FD_ZERO(&readfds);
            FD_ZERO(&writefds);
            int r = 0, w = 0;
            for (; !read_finish && r < FD_SETSIZE; ++r) {
                if (LUA_TNIL == lua_rawgeti(L, 1, x + r)) {
                    read_finish = true;
                    lua_pop(L, 1);
                    break;
                }

                socket::fd_t fd = checkfd(L, -1);
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
                socket::fd_t fd = checkfd(L, -1);
                MAXFD_SET(fd);
                FD_SET(fd, &writefds);
                lua_pop(L, 1);
            }
            int ok = ::select(MAXFD_GET(), &readfds, &writefds, EXFDS_GET(), timeop);
            if (ok > 0) {
                for (int i = 0; i < r; ++i) {
                    if (LUA_TUSERDATA == lua_rawgeti(L, 1, x + i) && FD_ISSET(tofd(L, -1), &readfds)) {
                        lua_rawseti(L, 4, ++rout);
                    }
                    else {
                        lua_pop(L, 1);
                    }
                }
                for (int i = 0; i < w; ++i) {
                    if (LUA_TUSERDATA == lua_rawgeti(L, 2, x + i) && FD_ISSET(tofd(L, -1), &writefds)) {
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
    static int luaopen(lua_State* L) {
        socket::initialize();
        luaL_Reg lib[] = {
            {"pair", pair},
            {"select", select},
            {"dump", dump},
            {"undump", undump},
            {"fd", fd},
            {NULL, NULL}
        };
        lua_newtable(L);
        luaL_setfuncs(L, lib, 0);
        luaL_Reg mt[] = {
            {"__call", __call},
            {NULL, NULL}
        };
        lua_newtable(L);
        luaL_setfuncs(L, mt, 0);
        lua_setmetatable(L, -2);
        return 1;
    }
}

DEFINE_LUAOPEN(socket)
