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

namespace bee::lua {
    template <>
    struct udata<net::socket::fd_t> {
        static inline auto name = "bee::socket";
    };
}

namespace bee::lua_socket {
    using namespace bee::net;
    static int push_neterror(lua_State* L, const char* msg) {
        auto error = make_neterror(msg);
        lua_pushnil(L);
        lua_pushstring(L, error.what());
        return 2;
    }
    static endpoint read_endpoint(lua_State* L, int idx) {
        auto ip = lua::checkstrview(L, idx);
        if (lua_isnoneornil(L, idx + 1)) {
            auto ep = endpoint::from_unixpath(ip);
            if (!ep.valid()) {
                luaL_error(L, "invalid address: %s", ip.data());
            }
            return ep;
        }
        else {
            auto port = lua::checkinteger<uint16_t>(L, idx + 1, "port");
            auto ep = endpoint::from_hostname(ip, port);
            if (!ep.valid()) {
                luaL_error(L, "invalid address: %s:%d", ip.data(), port);
            }
            return ep;
        }
    }
    static socket::fd_t tofd(lua_State* L, int idx) {
        return lua::toudata<socket::fd_t>(L, idx);
    }
    static socket::fd_t& checkfdref(lua_State* L, int idx) {
        return lua::checkudata<socket::fd_t>(L, idx);
    }
    static socket::fd_t checkfd(lua_State* L, int idx) {
        return lua::checkudata<socket::fd_t>(L, idx);
    }
    static void pushfd(lua_State* L, socket::fd_t fd);
    static int accept(lua_State* L) {
        auto fd = checkfd(L, 1);
        socket::fd_t newfd;
        if (socket::fdstat::success != socket::accept(fd, newfd)) {
            return push_neterror(L, "accept");
        }
        pushfd(L, newfd);
        return 1;
    }
    static int recv(lua_State* L) {
        auto fd = checkfd(L, 1);
        auto len = lua::optinteger<int>(L, 2, LUAL_BUFFERSIZE, "bufsize");
        luaL_Buffer b;
        luaL_buffinit(L, &b);
        char* buf = luaL_prepbuffsize(&b, (size_t)len);
        int rc;
        switch (socket::recv(fd, rc, buf, len)) {
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
        auto fd = checkfd(L, 1);
        auto buf = lua::checkstrview(L, 2);
        int rc;
        switch (socket::send(fd, rc, buf.data(), (int)buf.size())) {
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
        auto fd = checkfd(L, 1);
        auto len = lua::optinteger<int>(L, 2, LUAL_BUFFERSIZE, "bufsize");
        auto ep = endpoint::from_empty();
        luaL_Buffer b;
        luaL_buffinit(L, &b);
        char* buf = luaL_prepbuffsize(&b, (size_t)len);
        int   rc;
        switch (socket::recvfrom(fd, rc, buf, len, ep)) {
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
        auto fd = checkfd(L, 1);
        auto buf = lua::checkstrview(L, 2);
        auto ip = lua::checkstrview(L, 3);
        auto port = lua::checkinteger<uint16_t>(L, 4, "port");
        auto ep = endpoint::from_hostname(ip, port);
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
    static bool socket_destroy(lua_State* L) {
        auto& fd = checkfdref(L, 1);
        if (fd == socket::retired_fd) {
            return true;
        }
        bool ok = socket::close(fd);
        fd = socket::retired_fd;
        return ok;
    }
    static int close(lua_State* L) {
        if (!socket_destroy(L)) {
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
        auto fd = checkfd(L, 1);
        if (lua_isnoneornil(L, 2)) {
            return shutdown(L, fd, socket::shutdown_flag::both);
        }
        auto flag = lua::checkstrview(L, 2);
        switch (flag[0]) {
        case 'r':
            return shutdown(L, fd, socket::shutdown_flag::read);
        case 'w':
            return shutdown(L, fd, socket::shutdown_flag::write);
        default:
            lua_pushnil(L);
            lua_pushstring(L, "invalid flag");
            return 2;
        }
    }
    static int mt_tostring(lua_State* L) {
        auto fd = checkfd(L, 1);
        if (fd == socket::retired_fd) {
            lua_pushstring(L, "socket (closed)");
            return 1;
        }
        lua_pushfstring(L, "socket (%d)", fd);
        return 1;
    }
    static int mt_close(lua_State* L) {
        socket_destroy(L);
        return 0;
    }
    static int mt_gc(lua_State* L) {
        auto fd = checkfd(L, 1);
        if (fd != socket::retired_fd) {
            socket::close(fd);
        }
        return 0;
    }
    static int status(lua_State* L) {
        auto fd = checkfd(L, 1);
        int err = socket::errcode(fd);
        if (err == 0) {
            lua_pushboolean(L, 1);
            return 1;
        }
        auto error = make_error(err, "status");
        lua_pushnil(L);
        lua_pushstring(L, error.what());
        return 2;
    }
    static int info(lua_State* L) {
        auto fd = checkfd(L, 1);
        auto which = lua::checkstrview(L, 2);
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
        auto fd = checkfd(L, 1);
        lua_pushlightuserdata(L, (void*)(intptr_t)fd);
        return 1;
    }
    static int detach(lua_State* L) {
        auto& fd = checkfdref(L, 1);
        lua_pushlightuserdata(L, (void*)(intptr_t)fd);
        fd = socket::retired_fd;
        return 1;
    }
    static int option(lua_State* L) {
        auto fd = checkfd(L, 1);
        static const char *const opts[] = {"reuseaddr", "sndbuf", "rcvbuf", NULL};
        auto opt = (socket::option)luaL_checkoption(L, 2, NULL, opts);
        auto value = lua::checkinteger<int>(L, 3, "value");
        bool ok = socket::setoption(fd, opt, value);
        if (!ok) {
            return push_neterror(L, "setsockopt");
        }
        lua_pushboolean(L, 1);
        return 1;
    }
    static int connect(lua_State* L) {
        auto fd = checkfd(L, 1);
        auto ep = read_endpoint(L, 2);
        switch (socket::connect(fd, ep)) {
        case socket::fdstat::success:
            lua_pushboolean(L, 1);
            return 1;
        case socket::fdstat::wait:
            lua_pushboolean(L, 0);
            return 1;
        case socket::fdstat::failed:
            return push_neterror(L, "connect");
        default:
            std::unreachable();
        }
    }
    static int bind(lua_State* L) {
        auto fd = checkfd(L, 1);
        auto ep = read_endpoint(L, 2);
        socket::unlink(ep);
        if (!socket::bind(fd, ep)) {
            return push_neterror(L, "bind");
        }
        lua_pushboolean(L, 1);
        return 1;
    }
    static int listen(lua_State* L) {
        static const int kDefaultBackLog = 5;
        auto fd = checkfd(L, 1);
        auto backlog = lua::optinteger<int>(L, 2, kDefaultBackLog, "backlog");
        if (!socket::listen(fd, backlog)) {
            return push_neterror(L, "listen");
        }
        lua_pushboolean(L, 1);
        return 1;
    }
    static void metatable(lua_State* L) {
        luaL_Reg lib[] = {
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
            {"detach", detach},
            {"option", option},
            {NULL, NULL},
        };
        luaL_newlibtable(L, lib);
        luaL_setfuncs(L, lib, 0);
        lua_setfield(L, -2, "__index");
        luaL_Reg mt[] = {
            {"__tostring", mt_tostring},
            {"__close", mt_close},
            {"__gc", mt_gc},
            {NULL, NULL},
        };
        luaL_setfuncs(L, mt, 0);
    }
    static void pushfd(lua_State* L, socket::fd_t fd) {
        lua::newudata<socket::fd_t>(L, metatable, fd);
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
    static int fd(lua_State* L) {
        auto fd = lua::checklightud<socket::fd_t>(L, 1);
        pushfd(L, fd);
        return 1;
    }
    static int mt_call(lua_State* L) {
        static const char *const opts[] = {
            "tcp", "udp", "unix", "tcp6", "udp6",
            NULL
        };
        auto protocol = (socket::protocol)luaL_checkoption(L, 2, NULL, opts);
        auto fd = socket::open(protocol);
        if (fd == socket::retired_fd) {
            return push_neterror(L, "socket");
        }
        pushfd(L, fd);
        return 1;
    }
    struct select_wrap {
        select_wrap() { reset(); }
#if defined(_WIN32)
        void reset() { FD_ZERO(&readfds); FD_ZERO(&writefds); }
        void set_fd(socket::fd_t fd, fd_set& set) { FD_SET(fd, &set); }
        int select(struct timeval* timeop) { return ::select(0, &readfds, &writefds, &writefds, timeop); }
#else
        void reset() { FD_ZERO(&readfds); FD_ZERO(&writefds); maxfd = 0; }
        void set_fd(socket::fd_t fd, fd_set& set) { FD_SET(fd, &set);  maxfd = std::max(maxfd, fd); }
        int select(struct timeval* timeop) { return ::select(maxfd + 1, &readfds, &writefds, NULL, timeop); }
        int maxfd;
#endif
        fd_set readfds, writefds;
    };
    static int select(lua_State* L) {
        bool read_finish = true, write_finish = true;
        if (lua_type(L, 1) == LUA_TTABLE)
            read_finish = false;
        else if (!lua_isnoneornil(L, 1))
            luaL_typeerror(L, 1, lua_typename(L, LUA_TTABLE));
        if (lua_type(L, 2) == LUA_TTABLE)
            write_finish = false;
        else if (!lua_isnoneornil(L, 2))
            luaL_typeerror(L, 2, lua_typename(L, LUA_TTABLE));
        double timeo = luaL_optnumber(L, 3, -1);
        if (read_finish && write_finish) {
            if (timeo < 0) {
                return luaL_error(L, "no open sockets to check and no timeout set");
            }
            else {
                thread_sleep(static_cast<int>(timeo * 1000));
                lua_newtable(L);
                lua_newtable(L);
                return 2;
            }
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
        select_wrap wrap;
        for (int x = 1; !read_finish || !write_finish; x += FD_SETSIZE) {
            wrap.reset();
            int r = 0, w = 0;
            for (; !read_finish && r < FD_SETSIZE; ++r) {
                if (LUA_TNIL == lua_rawgeti(L, 1, x + r)) {
                    read_finish = true;
                    lua_pop(L, 1);
                    break;
                }
                wrap.set_fd(checkfd(L, -1), wrap.readfds);
                lua_pop(L, 1);
            }
            for (; !write_finish && w < FD_SETSIZE; ++w) {
                if (LUA_TNIL == lua_rawgeti(L, 2, x + w)) {
                    write_finish = true;
                    lua_pop(L, 1);
                    break;
                }
                wrap.set_fd(checkfd(L, -1), wrap.writefds);
                lua_pop(L, 1);
            }
            int ok = wrap.select(timeop);
            if (ok > 0) {
                for (int i = 0; i < r; ++i) {
                    if (LUA_TUSERDATA == lua_rawgeti(L, 1, x + i) && FD_ISSET(tofd(L, -1), &wrap.readfds)) {
                        lua_rawseti(L, 4, ++rout);
                    }
                    else {
                        lua_pop(L, 1);
                    }
                }
                for (int i = 0; i < w; ++i) {
                    if (LUA_TUSERDATA == lua_rawgeti(L, 2, x + i) && FD_ISSET(tofd(L, -1), &wrap.writefds)) {
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
        if (!socket::initialize()) {
            lua_pushstring(L, make_syserror("initialize").what());
            return lua_error(L);
        }
        luaL_Reg lib[] = {
            {"pair", pair},
            {"select", select},
            {"fd", fd},
            {NULL, NULL}
        };
        luaL_newlibtable(L, lib);
        luaL_setfuncs(L, lib, 0);
        luaL_Reg mt[] = {
            {"__call", mt_call},
            {NULL, NULL}
        };
        luaL_newlibtable(L, mt);
        luaL_setfuncs(L, mt, 0);
        lua_setmetatable(L, -2);
        return 1;
    }
}

DEFINE_LUAOPEN(socket)
