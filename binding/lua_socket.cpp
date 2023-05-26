#include <binding/binding.h>
#if defined _WIN32
#    include <winsock.h>

#    include <map>
#else
#    include <sys/select.h>
#endif
#include <bee/error.h>
#include <bee/net/endpoint.h>
#include <bee/net/socket.h>
#include <bee/nonstd/unreachable.h>
#include <bee/thread/simplethread.h>

namespace bee::lua {
    template <>
    struct udata<net::fd_t> {
        static inline auto name = "bee::net::fd";
    };
}

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
    static void pushfd(lua_State* L, net::fd_t fd) {
        lua::newudata<net::fd_t>(L, metatable, fd);
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
        auto fd = lua::checklightud<net::fd_t>(L, 1);
        pushfd(L, fd);
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
#if defined(_WIN32)
    struct socket_set {
        struct storage {
            uint32_t count;
            SOCKET array[1];
        };
        socket_set()
            : instack()
            , inheap(nullptr) {
            FD_ZERO(&instack);
        }
        socket_set(uint32_t n) {
            if (n > FD_SETSIZE) {
                inheap        = reinterpret_cast<storage*>(new uint8_t[sizeof(uint32_t) + sizeof(SOCKET) * n]);
                inheap->count = n;
            }
            else {
                FD_ZERO(&instack);
                inheap = nullptr;
            }
        }
        ~socket_set() {
            delete[] (reinterpret_cast<uint8_t*>(inheap));
        }
        fd_set* operator&() {
            if (inheap) {
                return reinterpret_cast<fd_set*>(inheap);
            }
            return &instack;
        }
        void set(SOCKET fd, lua_Integer i) {
            fd_set* set                    = &*this;
            set->fd_array[set->fd_count++] = fd;
            map[fd]                        = i;
        }
        fd_set instack;
        storage* inheap;
        std::map<SOCKET, lua_Integer> map;
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
        lua_Number timeo = luaL_optnumber(L, 3, -1);
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
            timeout.tv_sec  = (long)timeo;
            timeout.tv_usec = (long)((timeo - timeout.tv_sec) * 1000000);
        }
        lua_settop(L, 3);
        lua_Integer read_n  = read_finish ? 0 : luaL_len(L, 1);
        lua_Integer write_n = write_finish ? 0 : luaL_len(L, 2);
        socket_set readfds(static_cast<uint32_t>(read_n));
        socket_set writefds(static_cast<uint32_t>(write_n));
        for (lua_Integer i = 1; i <= read_n; ++i) {
            lua_rawgeti(L, 1, i);
            readfds.set(checkfd(L, -1), i);
            lua_pop(L, 1);
        }
        for (lua_Integer i = 1; i <= write_n; ++i) {
            lua_rawgeti(L, 2, i);
            writefds.set(checkfd(L, -1), i);
            lua_pop(L, 1);
        }
        int ok = ::select(0, &readfds, &writefds, &writefds, timeop);
        if (ok < 0) {
            return push_neterror(L, "select");
        }
        else if (ok == 0) {
            lua_newtable(L);
            lua_newtable(L);
            return 2;
        }
        else {
            {
                lua_newtable(L);
                auto set        = &readfds;
                lua_Integer out = 0;
                for (uint32_t i = 0; i < set->fd_count; ++i) {
                    SOCKET fd         = set->fd_array[i];
                    lua_Integer index = readfds.map[fd];
                    lua_rawgeti(L, 1, index);
                    lua_rawseti(L, 4, ++out);
                }
            }
            {
                lua_newtable(L);
                auto set        = &writefds;
                lua_Integer out = 0;
                for (uint32_t i = 0; i < set->fd_count; ++i) {
                    SOCKET fd         = set->fd_array[i];
                    lua_Integer index = writefds.map[fd];
                    lua_rawgeti(L, 2, index);
                    lua_rawseti(L, 5, ++out);
                }
            }
            return 2;
        }
    }
#else
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
        lua_Number timeo = luaL_optnumber(L, 3, -1);
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
            timeout.tv_sec  = (long)timeo;
            timeout.tv_usec = (long)((timeo - timeout.tv_sec) * 1000000);
        }
        lua_settop(L, 3);
        int maxfd = 0;
        fd_set readfds;
        fd_set writefds;
        FD_ZERO(&readfds);
        FD_ZERO(&writefds);
        lua_Integer read_n  = read_finish ? 0 : luaL_len(L, 1);
        lua_Integer write_n = write_finish ? 0 : luaL_len(L, 2);
        if (!read_finish) {
            for (lua_Integer i = 1; i <= read_n; ++i) {
                lua_rawgeti(L, 1, i);
                auto fd = checkfd(L, -1);
                FD_SET(fd, &readfds);
                maxfd = std::max(maxfd, fd);
                lua_pop(L, 1);
            }
        }
        if (!write_finish) {
            for (lua_Integer i = 1; i <= write_n; ++i) {
                lua_rawgeti(L, 2, i);
                auto fd = checkfd(L, -1);
                FD_SET(fd, &writefds);
                maxfd = std::max(maxfd, fd);
                lua_pop(L, 1);
            }
        }
        int ok;
        if (timeop == NULL) {
            do
                ok = ::select(maxfd + 1, &readfds, &writefds, NULL, NULL);
            while (ok == -1 && errno == EINTR);
        }
        else {
            ok = ::select(maxfd + 1, &readfds, &writefds, NULL, timeop);
            if (ok == -1 && errno == EINTR) {
                lua_newtable(L);
                lua_newtable(L);
                return 2;
            }
        }
        if (ok < 0) {
            return push_neterror(L, "select");
        }
        else if (ok == 0) {
            lua_newtable(L);
            lua_newtable(L);
            return 2;
        }
        else {
            lua_Integer rout = 0, wout = 0;
            lua_newtable(L);
            lua_newtable(L);
            for (lua_Integer i = 1; i <= read_n; ++i) {
                lua_rawgeti(L, 1, i);
                if (FD_ISSET(lua::toudata<net::fd_t>(L, -1), &readfds)) {
                    lua_rawseti(L, 4, ++rout);
                }
                else {
                    lua_pop(L, 1);
                }
            }
            for (lua_Integer i = 1; i <= write_n; ++i) {
                lua_rawgeti(L, 2, i);
                if (FD_ISSET(lua::toudata<net::fd_t>(L, -1), &writefds)) {
                    lua_rawseti(L, 5, ++wout);
                }
                else {
                    lua_pop(L, 1);
                }
            }
            return 2;
        }
    }
#endif
    static int luaopen(lua_State* L) {
        if (!net::socket::initialize()) {
            lua_pushstring(L, make_syserror("initialize").c_str());
            return lua_error(L);
        }
        luaL_Reg lib[] = {
            { "pair", pair },
            { "select", select },
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
