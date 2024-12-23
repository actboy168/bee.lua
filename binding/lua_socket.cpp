#include <bee/lua/binding.h>
#include <bee/lua/error.h>
#include <bee/lua/module.h>
#include <bee/lua/udata.h>
#include <bee/net/endpoint.h>
#include <bee/net/socket.h>
#include <bee/nonstd/unreachable.h>

namespace bee::lua_socket {
    namespace endpoint {
        static int value(lua_State* L) {
            const auto& ep = lua::checkudata<net::endpoint>(L, 1);
            switch (ep.get_family()) {
            case net::family::inet: {
                auto [ip, port] = ep.get_inet();
                lua_pushlstring(L, ip.data(), ip.size());
                lua_pushinteger(L, port);
                return 2;
            }
            case net::family::inet6: {
                auto [ip, port] = ep.get_inet6();
                lua_pushlstring(L, ip.data(), ip.size());
                lua_pushinteger(L, port);
                return 2;
            }
            case net::family::unix: {
                auto [type, path] = ep.get_unix();
                lua_pushlstring(L, path.data(), path.size());
                lua_pushinteger(L, std::to_underlying(type));
                return 2;
            }
            case net::family::unknown:
                return 0;
            default:
                std::unreachable();
            }
        }
        static int mt_tostring(lua_State* L) {
            const auto& ep = lua::checkudata<net::endpoint>(L, 1);
            switch (ep.get_family()) {
            case net::family::inet: {
                auto [ip, port] = ep.get_inet();
                lua_pushfstring(L, "%s:%d", ip.data(), port);
                return 1;
            }
            case net::family::inet6: {
                auto [ip, port] = ep.get_inet6();
                lua_pushfstring(L, "%s:%d", ip.data(), port);
                return 1;
            }
            case net::family::unix: {
                auto [type, path] = ep.get_unix();
                lua_pushlstring(L, path.data(), path.size());
                return 1;
            }
            case net::family::unknown:
                lua_pushstring(L, "<unknown>");
                return 1;
            default:
                std::unreachable();
            }
        }
        static int mt_eq(lua_State* L) {
            const auto& a = lua::checkudata<net::endpoint>(L, 1);
            const auto& b = lua::checkudata<net::endpoint>(L, 2);
            lua_pushboolean(L, a == b);
            return 1;
        }
        static void metatable(lua_State* L) {
            luaL_Reg lib[] = {
                { "value", value },
                { NULL, NULL },
            };
            luaL_newlibtable(L, lib);
            luaL_setfuncs(L, lib, 0);
            lua_setfield(L, -2, "__index");
            luaL_Reg mt[] = {
                { "__tostring", mt_tostring },
                { "__eq", mt_eq },
                { NULL, NULL },
            };
            luaL_setfuncs(L, mt, 0);
        }
    }

    struct fd_no_ownership {
        net::fd_t v;
        fd_no_ownership(net::fd_t v) noexcept
            : v(v) {}
        operator net::fd_t() const noexcept {
            return v;
        }
    };
    namespace fd {
        static net::endpoint& to_endpoint(lua_State* L, int idx, net::endpoint& ep) {
            switch (lua_type(L, idx)) {
            case LUA_TSTRING:
                if (lua_isnoneornil(L, idx + 1)) {
                    auto path = lua::checkstrview(L, idx);
                    if (!net::endpoint::ctor_unix(ep, path)) {
                        luaL_error(L, "invalid address: %s", path.data());
                    }
                    return ep;
                } else {
                    auto name = lua::checkstrview(L, idx);
                    auto port = lua::checkinteger<uint16_t>(L, idx + 1);
                    if (!net::endpoint::ctor_hostname(ep, name, port)) {
                        luaL_error(L, "invalid address: %s:%d", name.data(), port);
                    }
                    return ep;
                }
            default:
            case LUA_TUSERDATA:
                return lua::checkudata<net::endpoint>(L, idx);
            }
        }

        static int accept(lua_State* L, net::fd_t fd) {
            net::fd_t newfd;
            switch (net::socket::accept(fd, newfd)) {
            case net::socket::status::wait:
                lua_pushboolean(L, 0);
                return 1;
            case net::socket::status::success:
                lua::newudata<net::fd_t>(L, newfd);
                return 1;
            case net::socket::status::failed:
                return lua::return_net_error(L, "accept");
            default:
                std::unreachable();
            }
        }
        static int recv(lua_State* L, net::fd_t fd) {
            auto len = lua::optinteger<int, LUAL_BUFFERSIZE>(L, 2);
            luaL_Buffer b;
            luaL_buffinit(L, &b);
            char* buf = luaL_prepbuffsize(&b, (size_t)len);
            int rc;
            switch (net::socket::recv(fd, rc, buf, len)) {
            case net::socket::recv_status::close:
                lua_pushnil(L);
                return 1;
            case net::socket::recv_status::wait:
                lua_pushboolean(L, 0);
                return 1;
            case net::socket::recv_status::success:
                luaL_pushresultsize(&b, rc);
                return 1;
            case net::socket::recv_status::failed:
                return lua::return_net_error(L, "recv");
            default:
                std::unreachable();
            }
        }
        static int send(lua_State* L, net::fd_t fd) {
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
                return lua::return_net_error(L, "send");
            default:
                std::unreachable();
            }
        }
        static int recvfrom(lua_State* L, net::fd_t fd) {
            auto len = lua::optinteger<int, LUAL_BUFFERSIZE>(L, 2);
            auto& ep = lua::newudata<net::endpoint>(L);
            luaL_Buffer b;
            luaL_buffinit(L, &b);
            char* buf = luaL_prepbuffsize(&b, (size_t)len);
            int rc;
            switch (net::socket::recvfrom(fd, rc, ep, buf, len)) {
            case net::socket::status::success:
                luaL_pushresultsize(&b, rc);
                lua_insert(L, -2);
                return 2;
            case net::socket::status::wait:
                lua_pushboolean(L, 0);
                return 1;
            case net::socket::status::failed:
                return lua::return_net_error(L, "recvfrom");
            default:
                std::unreachable();
            }
        }
        static int sendto(lua_State* L, net::fd_t fd) {
            auto buf = lua::checkstrview(L, 2);
            net::endpoint stack_ep;
            auto& ep = to_endpoint(L, 3, stack_ep);
            int rc;
            switch (net::socket::sendto(fd, rc, buf.data(), (int)buf.size(), ep)) {
            case net::socket::status::wait:
                lua_pushboolean(L, 0);
                return 1;
            case net::socket::status::success:
                lua_pushinteger(L, rc);
                return 1;
            case net::socket::status::failed:
                return lua::return_net_error(L, "sendto");
            default:
                std::unreachable();
            }
        }
        static int shutdown(lua_State* L, net::fd_t fd, net::socket::shutdown_flag flag) {
            if (!net::socket::shutdown(fd, flag)) {
                return lua::return_net_error(L, "shutdown");
            }
            lua_pushboolean(L, 1);
            return 1;
        }
        static int shutdown(lua_State* L, net::fd_t fd) {
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
                return lua::return_error(L, "invalid flag");
            }
        }
        static int status(lua_State* L, net::fd_t fd) {
            int err = 0;
            if (!net::socket::errcode(fd, err)) {
                return lua::return_net_error(L, "getsockopt(SO_ERROR)");
            }
            if (!err) {
                lua_pushboolean(L, 1);
                return 1;
            }
            return lua::return_net_error(L, "status", err);
        }
        static int info(lua_State* L, net::fd_t fd) {
            auto which = lua::checkstrview(L, 2);
            if (which == "peer") {
                auto& ep = lua::newudata<net::endpoint>(L);
                if (net::socket::getpeername(fd, ep)) {
                    return 1;
                }
                return lua::return_net_error(L, "getpeername");
            } else if (which == "socket") {
                auto& ep = lua::newudata<net::endpoint>(L);
                if (net::socket::getsockname(fd, ep)) {
                    return 1;
                }
                return lua::return_net_error(L, "getsockname");
            }
            return 0;
        }
        static int handle(lua_State* L, net::fd_t fd) {
            lua_pushlightuserdata(L, (void*)(intptr_t)fd);
            return 1;
        }
        static int option(lua_State* L, net::fd_t fd) {
            static const char* const opts[] = { "reuseaddr", "sndbuf", "rcvbuf", NULL };
            auto opt                        = (net::socket::option)luaL_checkoption(L, 2, NULL, opts);
            auto value                      = lua::checkinteger<int>(L, 3);
            bool ok                         = net::socket::setoption(fd, opt, value);
            if (!ok) {
                return lua::return_net_error(L, "setsockopt");
            }
            lua_pushboolean(L, 1);
            return 1;
        }
        static int connect(lua_State* L, net::fd_t fd) {
            net::endpoint stack_ep;
            const auto& ep = to_endpoint(L, 2, stack_ep);
            switch (net::socket::connect(fd, ep)) {
            case net::socket::status::success:
                lua_pushboolean(L, 1);
                return 1;
            case net::socket::status::wait:
                lua_pushboolean(L, 0);
                return 1;
            case net::socket::status::failed:
                return lua::return_net_error(L, "connect");
            default:
                std::unreachable();
            }
        }
        static int bind(lua_State* L, net::fd_t fd) {
            net::endpoint stack_ep;
            const auto& ep = to_endpoint(L, 2, stack_ep);
            if (!net::socket::bind(fd, ep)) {
                return lua::return_net_error(L, "bind");
            }
            lua_pushboolean(L, 1);
            return 1;
        }
        static int listen(lua_State* L, net::fd_t fd) {
            constexpr int kDefaultBackLog = 5;
            auto backlog                  = lua::optinteger<int, kDefaultBackLog>(L, 2);
            if (!net::socket::listen(fd, backlog)) {
                return lua::return_net_error(L, "listen");
            }
            lua_pushboolean(L, 1);
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
        static int mt_tostring(lua_State* L) {
            auto fd = lua::checkudata<net::fd_t>(L, 1);
            if (fd == net::retired_fd) {
                lua_pushstring(L, "socket (closed)");
                return 1;
            }
            lua_pushfstring(L, "socket (%d)", fd);
            return 1;
        }
        static int mt_tostring_no_ownership(lua_State* L) {
            auto fd = (net::fd_t)lua::checkudata<fd_no_ownership>(L, 1);
            if (fd == net::retired_fd) {
                lua_pushstring(L, "socket (closed) (no ownership)");
                return 1;
            }
            lua_pushfstring(L, "socket (%d) (no ownership)", fd);
            return 1;
        }
        static int close(lua_State* L) {
            auto& fd = lua::checkudata<net::fd_t>(L, 1);
            if (fd != net::retired_fd) {
                if (!net::socket::close(fd)) {
                    fd = net::retired_fd;
                    return lua::return_net_error(L, "close");
                }
                fd = net::retired_fd;
            }
            lua_pushboolean(L, 1);
            return 1;
        }
        static int mt_close(lua_State* L) {
            auto& fd = lua::checkudata<net::fd_t>(L, 1);
            if (fd != net::retired_fd) {
                net::socket::close(fd);
                fd = net::retired_fd;
            }
            return 0;
        }
        static int mt_gc(lua_State* L) {
            auto fd = lua::checkudata<net::fd_t>(L, 1);
            if (fd != net::retired_fd) {
                net::socket::close(fd);
            }
            return 0;
        }
        using socket_func = int (*)(lua_State*, net::fd_t);
        template <socket_func func, typename T = net::fd_t>
        static int call_socket(lua_State* L) {
            auto fd = (net::fd_t)lua::checkudata<T>(L, 1);
            if (fd == net::retired_fd) {
                luaL_error(L, "socket is already closed.");
            }
            return func(L, fd);
        }
        static void metatable(lua_State* L) {
            luaL_Reg lib[] = {
                { "connect", call_socket<connect> },
                { "bind", call_socket<bind> },
                { "listen", call_socket<listen> },
                { "accept", call_socket<accept> },
                { "recv", call_socket<recv> },
                { "send", call_socket<send> },
                { "recvfrom", call_socket<recvfrom> },
                { "sendto", call_socket<sendto> },
                { "shutdown", call_socket<shutdown> },
                { "status", call_socket<status> },
                { "info", call_socket<info> },
                { "option", call_socket<option> },
                { "handle", call_socket<handle> },
                { "detach", detach },
                { "close", close },
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
                { "connect", call_socket<connect, fd_no_ownership> },
                { "bind", call_socket<bind, fd_no_ownership> },
                { "listen", call_socket<listen, fd_no_ownership> },
                { "accept", call_socket<accept, fd_no_ownership> },
                { "recv", call_socket<recv, fd_no_ownership> },
                { "send", call_socket<send, fd_no_ownership> },
                { "recvfrom", call_socket<recvfrom, fd_no_ownership> },
                { "sendto", call_socket<sendto, fd_no_ownership> },
                { "shutdown", call_socket<shutdown, fd_no_ownership> },
                { "status", call_socket<status, fd_no_ownership> },
                { "info", call_socket<info, fd_no_ownership> },
                { "option", call_socket<option, fd_no_ownership> },
                { "handle", call_socket<handle, fd_no_ownership> },
                { NULL, NULL },
            };
            luaL_newlibtable(L, lib);
            luaL_setfuncs(L, lib, 0);
            lua_setfield(L, -2, "__index");
            luaL_Reg mt[] = {
                { "__tostring", mt_tostring_no_ownership },
                { NULL, NULL },
            };
            luaL_setfuncs(L, mt, 0);
        }
    }

    static int l_create(lua_State* L) {
        static const char* const opts[] = {
            "tcp", "udp", "unix", "tcp6", "udp6",
            NULL
        };
        auto protocol = (net::socket::protocol)luaL_checkoption(L, 1, NULL, opts);
        auto fd       = net::socket::open(protocol);
        if (fd == net::retired_fd) {
            return lua::return_net_error(L, "socket");
        }
        lua::newudata<net::fd_t>(L, fd);
        return 1;
    }
    static int l_endpoint(lua_State* L) {
        enum class endpoint_ctor {
            unix,
            hostname,
            inet,
            inet6,
        };
        static const char* const opts[] = {
            "unix",
            "hostname",
            "inet",
            "inet6",
            NULL
        };
        switch ((endpoint_ctor)luaL_checkoption(L, 1, NULL, opts)) {
        case endpoint_ctor::unix: {
            auto path = lua::checkstrview(L, 2);
            auto& ep  = lua::newudata<net::endpoint>(L);
            if (!net::endpoint::ctor_unix(ep, path)) {
                return 0;
            }
            return 1;
        }
        case endpoint_ctor::hostname: {
            auto name = lua::checkstrview(L, 2);
            auto port = lua::checkinteger<uint16_t>(L, 3);
            auto& ep  = lua::newudata<net::endpoint>(L);
            if (!net::endpoint::ctor_hostname(ep, name, port)) {
                return 0;
            }
            return 1;
        }
        case endpoint_ctor::inet: {
            auto ip   = lua::checkstrview(L, 2);
            auto port = lua::checkinteger<uint16_t>(L, 3);
            auto& ep  = lua::newudata<net::endpoint>(L);
            if (!net::endpoint::ctor_inet(ep, ip, port)) {
                return 0;
            }
            return 1;
        }
        case endpoint_ctor::inet6: {
            auto ip   = lua::checkstrview(L, 2);
            auto port = lua::checkinteger<uint16_t>(L, 3);
            auto& ep  = lua::newudata<net::endpoint>(L);
            if (!net::endpoint::ctor_inet6(ep, ip, port)) {
                return 0;
            }
            return 1;
        }
        default:
            std::unreachable();
        }
    }
    static int l_pair(lua_State* L) {
        net::fd_t sv[2];
        if (!net::socket::pair(sv)) {
            return lua::return_net_error(L, "socketpair");
        }
        lua::newudata<net::fd_t>(L, sv[0]);
        lua::newudata<net::fd_t>(L, sv[1]);
        return 2;
    }
    static int l_fd(lua_State* L) {
        auto fd           = lua::checklightud<net::fd_t>(L, 1);
        bool no_ownership = lua_toboolean(L, 2);
        if (no_ownership) {
            lua::newudata<fd_no_ownership>(L, fd);
        } else {
            lua::newudata<net::fd_t>(L, fd);
        }
        return 1;
    }

    static int luaopen(lua_State* L) {
        if (!net::socket::initialize()) {
            lua::push_sys_error(L, "initialize");
            return lua_error(L);
        }
        luaL_Reg lib[] = {
            { "create", l_create },
            { "endpoint", l_endpoint },
            { "pair", l_pair },
            { "fd", l_fd },
            { NULL, NULL }
        };
        luaL_newlibtable(L, lib);
        luaL_setfuncs(L, lib, 0);
        return 1;
    }
}

DEFINE_LUAOPEN(socket)

namespace bee::lua {
    template <>
    struct udata<net::fd_t> {
        static inline auto metatable = bee::lua_socket::fd::metatable;
    };
    template <>
    struct udata<lua_socket::fd_no_ownership> {
        static inline auto metatable = bee::lua_socket::fd::metatable_no_ownership;
    };
    template <>
    struct udata<net::endpoint> {
        static inline auto metatable = bee::lua_socket::endpoint::metatable;
    };
}
