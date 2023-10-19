#include <bee/error.h>
#include <bee/net/endpoint.h>
#include <bee/net/socket.h>
#include <bee/nonstd/unreachable.h>
#include <binding/binding.h>

namespace bee::lua_socket {
    static int push_neterror(lua_State* L, std::string_view msg) {
        auto error = make_neterror(msg);
        lua_pushnil(L);
        lua_pushstring(L, error.c_str());
        return 2;
    }
    static net::endpoint check_endpoint(lua_State* L, int idx) {
        auto ip = lua::checkstrview(L, idx);
        if (lua_isnoneornil(L, idx + 1)) {
            auto ep = net::endpoint::from_unixpath(ip);
            if (!ep.valid()) {
                luaL_error(L, "invalid address: %s", ip.data());
            }
            return ep;
        }
        else {
            auto port = lua::checkinteger<uint16_t>(L, idx + 1);
            auto ep   = net::endpoint::from_hostname(ip, port);
            if (!ep.valid()) {
                luaL_error(L, "invalid address: %s:%d", ip.data(), port);
            }
            return ep;
        }
    }
    static net::fd_t checkfd(lua_State* L, int idx) {
        net::fd_t fd = lua::checkudata<net::fd_t>(L, idx);
        if (fd == net::retired_fd) {
            luaL_error(L, "socket is already closed.");
        }
        return fd;
    }
    static void pushfd(lua_State* L, net::fd_t fd);
    static int accept(lua_State* L) {
        auto fd = checkfd(L, 1);
        net::fd_t newfd;
        if (net::socket::fdstat::success != net::socket::accept(fd, newfd)) {
            return push_neterror(L, "accept");
        }
        pushfd(L, newfd);
        return 1;
    }
    static int recv(lua_State* L) {
        auto fd  = checkfd(L, 1);
        auto len = lua::optinteger<int, LUAL_BUFFERSIZE>(L, 2);
        luaL_Buffer b;
        luaL_buffinit(L, &b);
        char* buf = luaL_prepbuffsize(&b, (size_t)len);
        int rc;
        switch (net::socket::recv(fd, rc, buf, len)) {
        case net::socket::status::close:
            lua_pushnil(L);
            return 1;
        case net::socket::status::wait:
            lua_pushboolean(L, 0);
            return 1;
        case net::socket::status::success:
            luaL_pushresultsize(&b, rc);
            return 1;
        case net::socket::status::failed:
            return push_neterror(L, "recv");
        default:
            std::unreachable();
        }
    }
    static int send(lua_State* L) {
        auto fd  = checkfd(L, 1);
        auto buf = lua::checkstrview(L, 2);
        int rc;
        switch (net::socket::send(fd, rc, buf.data(), (int)buf.size())) {
        case net::socket::status::wait:
            lua_pushboolean(L, 0);
            return 1;
        case net::socket::status::success:
            lua_pushinteger(L, rc);
            return 1;
        case net::socket::status::failed:
            return push_neterror(L, "send");
        default:
            std::unreachable();
        }
    }
    static int recvfrom(lua_State* L) {
        auto fd  = checkfd(L, 1);
        auto len = lua::optinteger<int, LUAL_BUFFERSIZE>(L, 2);
        luaL_Buffer b;
        luaL_buffinit(L, &b);
        char* buf = luaL_prepbuffsize(&b, (size_t)len);
        int rc;
        auto res = net::socket::recvfrom(fd, rc, buf, len);
        if (res) {
            auto& ep = res.value();
            luaL_pushresultsize(&b, rc);
            auto [ip, port] = ep.info();
            lua_pushlstring(L, ip.data(), ip.size());
            lua_pushinteger(L, port);
            return 3;
        }
        switch (res.error()) {
        case net::socket::status::close:
            lua_pushnil(L);
            return 1;
        case net::socket::status::wait:
            lua_pushboolean(L, 0);
            return 1;
        case net::socket::status::failed:
            return push_neterror(L, "recvfrom");
        default:
            std::unreachable();
        }
    }
    static int sendto(lua_State* L) {
        auto fd   = checkfd(L, 1);
        auto buf  = lua::checkstrview(L, 2);
        auto ip   = lua::checkstrview(L, 3);
        auto port = lua::checkinteger<uint16_t>(L, 4);
        auto ep   = net::endpoint::from_hostname(ip, port);
        if (!ep.valid()) {
            return luaL_error(L, "invalid address: %s:%d", ip, port);
        }
        int rc;
        switch (net::socket::sendto(fd, rc, buf.data(), (int)buf.size(), ep)) {
        case net::socket::status::wait:
            lua_pushboolean(L, 0);
            return 1;
        case net::socket::status::success:
            lua_pushinteger(L, rc);
            return 1;
        case net::socket::status::failed:
            return push_neterror(L, "sendto");
        default:
            std::unreachable();
        }
    }
    static bool socket_destroy(lua_State* L) {
        auto& fd = lua::checkudata<net::fd_t>(L, 1);
        if (fd == net::retired_fd) {
            return true;
        }
        bool ok = net::socket::close(fd);
        fd      = net::retired_fd;
        return ok;
    }
    static int close(lua_State* L) {
        if (!socket_destroy(L)) {
            return push_neterror(L, "close");
        }
        lua_pushboolean(L, 1);
        return 1;
    }
    static int shutdown(lua_State* L, net::fd_t fd, net::socket::shutdown_flag flag) {
        if (!net::socket::shutdown(fd, flag)) {
            return push_neterror(L, "shutdown");
        }
        lua_pushboolean(L, 1);
        return 1;
    }
    static int shutdown(lua_State* L) {
        auto fd = checkfd(L, 1);
        if (lua_isnoneornil(L, 2)) {
            return shutdown(L, fd, net::socket::shutdown_flag::both);
        }
        auto flag = lua::checkstrview(L, 2);
        switch (flag[0]) {
        case 'r':
            return shutdown(L, fd, net::socket::shutdown_flag::read);
        case 'w':
            return shutdown(L, fd, net::socket::shutdown_flag::write);
        default:
            lua_pushnil(L);
            lua_pushstring(L, "invalid flag");
            return 2;
        }
    }
    static int mt_tostring(lua_State* L) {
        auto fd = lua::checkudata<net::fd_t>(L, 1);
        if (fd == net::retired_fd) {
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
        auto fd = lua::checkudata<net::fd_t>(L, 1);
        if (fd != net::retired_fd) {
            net::socket::close(fd);
        }
        return 0;
    }
    static int status(lua_State* L) {
        auto fd = checkfd(L, 1);
        auto ec = net::socket::errcode(fd);
        if (!ec) {
            lua_pushboolean(L, 1);
            return 1;
        }
        auto error = make_error(ec, "status");
        lua_pushnil(L);
        lua_pushstring(L, error.c_str());
        return 2;
    }
    static int info(lua_State* L) {
        auto fd    = checkfd(L, 1);
        auto which = lua::checkstrview(L, 2);
        if (which == "peer") {
            if (auto ep_opt = net::socket::getpeername(fd)) {
                auto [ip, port] = ep_opt->info();
                lua_pushlstring(L, ip.data(), ip.size());
                lua_pushinteger(L, port);
                return 2;
            }
            return push_neterror(L, "getpeername");
        }
        else if (which == "socket") {
            if (auto ep_opt = net::socket::getsockname(fd)) {
                auto [ip, port] = ep_opt->info();
                lua_pushlstring(L, ip.data(), ip.size());
                lua_pushinteger(L, port);
                return 2;
            }
            return push_neterror(L, "getsockname");
        }
        return 0;
    }
    static int handle(lua_State* L) {
        auto fd = checkfd(L, 1);
        lua_pushlightuserdata(L, (void*)(intptr_t)fd);
        return 1;
    }
    static int detach(lua_State* L) {
        auto& fd = lua::checkudata<net::fd_t>(L, 1);
        if (fd == net::retired_fd) {
            luaL_error(L, "socket is already closed.");
            return 0;
        }
        lua_pushlightuserdata(L, (void*)(intptr_t)fd);
        fd = net::retired_fd;
        return 1;
    }
    static int option(lua_State* L) {
        auto fd                         = checkfd(L, 1);
        static const char* const opts[] = { "reuseaddr", "sndbuf", "rcvbuf", NULL };
        auto opt                        = (net::socket::option)luaL_checkoption(L, 2, NULL, opts);
        auto value                      = lua::checkinteger<int>(L, 3);
        bool ok                         = net::socket::setoption(fd, opt, value);
        if (!ok) {
            return push_neterror(L, "setsockopt");
        }
        lua_pushboolean(L, 1);
        return 1;
    }
    static int connect(lua_State* L) {
        auto fd = checkfd(L, 1);
        auto ep = check_endpoint(L, 2);
        switch (net::socket::connect(fd, ep)) {
        case net::socket::fdstat::success:
            lua_pushboolean(L, 1);
            return 1;
        case net::socket::fdstat::wait:
            lua_pushboolean(L, 0);
            return 1;
        case net::socket::fdstat::failed:
            return push_neterror(L, "connect");
        default:
            std::unreachable();
        }
    }
    static int bind(lua_State* L) {
        auto fd = checkfd(L, 1);
        auto ep = check_endpoint(L, 2);
        net::socket::unlink(ep);
        if (!net::socket::bind(fd, ep)) {
            return push_neterror(L, "bind");
        }
        lua_pushboolean(L, 1);
        return 1;
    }
    static int listen(lua_State* L) {
        static constexpr int kDefaultBackLog = 5;
        auto fd                              = checkfd(L, 1);
        auto backlog                         = lua::optinteger<int, kDefaultBackLog>(L, 2);
        if (!net::socket::listen(fd, backlog)) {
            return push_neterror(L, "listen");
        }
        lua_pushboolean(L, 1);
        return 1;
    }
    static void metatable(lua_State* L) {
        luaL_Reg lib[] = {
            { "connect", connect },
            { "bind", bind },
            { "listen", listen },
            { "accept", accept },
            { "recv", recv },
            { "send", send },
            { "recvfrom", recvfrom },
            { "sendto", sendto },
            { "close", close },
            { "shutdown", shutdown },
            { "status", status },
            { "info", info },
            { "handle", handle },
            { "detach", detach },
            { "option", option },
            { NULL, NULL },
        };
        luaL_newlibtable(L, lib);
        luaL_setfuncs(L, lib, 0);
        lua_setfield(L, -2, "__index");
        luaL_Reg mt[] = {
            { "__tostring", mt_tostring },
            { "__close", mt_close },
            { "__gc", mt_gc },
            { NULL, NULL },
        };
        luaL_setfuncs(L, mt, 0);
    }
    static void metatable_no_ownership(lua_State* L) {
        luaL_Reg lib[] = {
            { "connect", connect },
            { "bind", bind },
            { "listen", listen },
            { "accept", accept },
            { "recv", recv },
            { "send", send },
            { "recvfrom", recvfrom },
            { "sendto", sendto },
            { "shutdown", shutdown },
            { "status", status },
            { "info", info },
            { "handle", handle },
            { "detach", detach },
            { "option", option },
            { NULL, NULL },
        };
        luaL_newlibtable(L, lib);
        luaL_setfuncs(L, lib, 0);
        lua_setfield(L, -2, "__index");
        luaL_Reg mt[] = {
            { "__tostring", mt_tostring },
            { NULL, NULL },
        };
        luaL_setfuncs(L, mt, 0);
    }
    static void pushfd(lua_State* L, net::fd_t fd) {
        lua::newudata<net::fd_t>(L, "bee::net::fd", metatable, fd);
    }
    static void pushfd_no_ownership(lua_State* L, net::fd_t fd) {
        lua::newudata<net::fd_t>(L, "bee::net::fd (no ownership)", metatable_no_ownership, fd);
    }
    static int pair(lua_State* L) {
        net::fd_t sv[2];
        if (!net::socket::pair(sv)) {
            return push_neterror(L, "socketpair");
        }
        pushfd(L, sv[0]);
        pushfd(L, sv[1]);
        return 2;
    }
    static int fd(lua_State* L) {
        auto fd           = lua::checklightud<net::fd_t>(L, 1);
        bool no_ownership = lua_toboolean(L, 2);
        if (no_ownership) {
            pushfd_no_ownership(L, fd);
        }
        else {
            pushfd(L, fd);
        }
        return 1;
    }
    static int mt_call(lua_State* L) {
        static const char* const opts[] = {
            "tcp", "udp", "unix", "tcp6", "udp6",
            NULL
        };
        auto protocol = (net::socket::protocol)luaL_checkoption(L, 2, NULL, opts);
        auto fd       = net::socket::open(protocol);
        if (fd == net::retired_fd) {
            return push_neterror(L, "socket");
        }
        pushfd(L, fd);
        return 1;
    }

    static int luaopen(lua_State* L) {
        if (!net::socket::initialize()) {
            lua_pushstring(L, make_syserror("initialize").c_str());
            return lua_error(L);
        }
        luaL_Reg lib[] = {
            { "pair", pair },
            { "fd", fd },
            { NULL, NULL }
        };
        luaL_newlibtable(L, lib);
        luaL_setfuncs(L, lib, 0);
        luaL_Reg mt[] = {
            { "__call", mt_call },
            { NULL, NULL }
        };
        luaL_newlibtable(L, mt);
        luaL_setfuncs(L, mt, 0);
        lua_setmetatable(L, -2);
        return 1;
    }
}

DEFINE_LUAOPEN(socket)
