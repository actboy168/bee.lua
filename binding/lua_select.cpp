#include <bee/lua/binding.h>
#include <bee/lua/module.h>
#include <bee/lua/udata.h>
#if defined(_WIN32)
#    include <winsock.h>
#else
#    include <sys/select.h>
#endif
#include <bee/lua/error.h>
#include <bee/net/socket.h>
#include <bee/thread/simplethread.h>

#include <set>

namespace bee::lua_select {
#if defined(_WIN32)
    struct socket_set {
        struct storage {
            uint32_t fd_count;
            SOCKET fd_array[1];
        };
        socket_set()
            : set(nullptr)
            , cap(0) {
            reset(FD_SETSIZE);
        }
        ~socket_set() {
            clear();
        }
        void clear() {
            if (set) {
                delete[] (reinterpret_cast<uint8_t*>(set));
            }
        }
        void reset(size_t n) {
            if (n > cap) {
                clear();
                set = reinterpret_cast<storage*>(new uint8_t[sizeof(uint32_t) + sizeof(SOCKET) * n]);
                cap = n;
            }
            set->fd_count = 0;
        }
        fd_set* ptr() {
            return reinterpret_cast<fd_set*>(set);
        }
        void insert(SOCKET fd) {
            set->fd_array[set->fd_count++] = fd;
        }
        storage* set;
        size_t cap;
    };

#endif

    struct select_ctx {
        std::set<net::fd_t> readset;
        std::set<net::fd_t> writeset;
#if defined(_WIN32)
        socket_set readfds;
        socket_set writefds;
        unsigned int i;
#else
        fd_set readfds;
        fd_set writefds;
        int maxfd;
        int i;
#endif
    };
    constexpr lua_Integer SELECT_READ  = 1;
    constexpr lua_Integer SELECT_WRITE = 2;
    static void storeref(lua_State* L, net::fd_t k) {
        if (lua_isnoneornil(L, 4)) {
            lua_getiuservalue(L, 1, 1);
            lua_pushinteger(L, (lua_Integer)k);
            lua_pushvalue(L, 2);
            lua_rawset(L, -3);
            lua_pop(L, 1);
            return;
        }
        lua_getiuservalue(L, 1, 1);
        lua_pushinteger(L, (lua_Integer)k);
        lua_pushvalue(L, 4);
        lua_rawset(L, -3);
        lua_pop(L, 1);
        lua_getiuservalue(L, 1, 2);
        lua_pushvalue(L, 2);
        lua_pushboolean(L, 1);
        lua_rawset(L, -3);
        lua_pop(L, 1);
    }
    static void cleanref(lua_State* L, net::fd_t k) {
        lua_getiuservalue(L, 1, 1);
        lua_pushinteger(L, (lua_Integer)k);
        lua_pushnil(L);
        lua_rawset(L, -3);
        lua_pop(L, 1);
        lua_getiuservalue(L, 1, 2);
        lua_pushvalue(L, 2);
        lua_pushnil(L);
        lua_rawset(L, -3);
        lua_pop(L, 1);
    }
    static void findref(lua_State* L, int t, net::fd_t k) {
        lua_getiuservalue(L, t, 1);
        lua_pushinteger(L, (lua_Integer)k);
        lua_rawget(L, -2);
        lua_remove(L, -2);
    }
    static int pairs_events(lua_State* L) {
        auto& ctx = lua::toudata<select_ctx>(L, lua_upvalueindex(1));
#if defined(_WIN32)
        auto rset = ctx.readfds.ptr();
        auto wset = ctx.writefds.ptr();
        auto rlen = rset->fd_count;
        if (ctx.i < rset->fd_count) {
            auto fd = rset->fd_array[ctx.i];
            findref(L, lua_upvalueindex(1), (net::fd_t)fd);
            lua_pushinteger(L, SELECT_READ);
            ++ctx.i;
            return 2;
        }
        if (ctx.i < rlen + wset->fd_count) {
            auto fd = wset->fd_array[ctx.i - rlen];
            findref(L, lua_upvalueindex(1), (net::fd_t)fd);
            lua_pushinteger(L, SELECT_WRITE);
            ++ctx.i;
            return 2;
        }
#else
        for (; ctx.i <= ctx.maxfd; ++ctx.i) {
            lua_Integer event = 0;
            if (FD_ISSET(ctx.i, &ctx.readfds)) {
                event |= SELECT_READ;
            }
            if (FD_ISSET(ctx.i, &ctx.writefds)) {
                event |= SELECT_WRITE;
            }
            if (event) {
                findref(L, lua_upvalueindex(1), (net::fd_t)ctx.i);
                lua_pushinteger(L, event);
                ++ctx.i;
                return 2;
            }
        }
#endif
        return 0;
    }
    static int empty_events(lua_State* L) {
        return 0;
    }
    static int wait(lua_State* L) {
        auto& ctx = lua::checkudata<select_ctx>(L, 1);
        int msec  = lua::optinteger<int, -1>(L, 2);
        if (ctx.readset.empty() && ctx.writeset.empty()) {
            if (msec < 0) {
                return luaL_error(L, "no open sockets to check and no timeout set");
            } else {
                thread_sleep(msec);
                lua_getiuservalue(L, 1, 4);
                return 1;
            }
        }
        struct timeval timeout, *timeop = &timeout;
        if (msec < 0) {
            timeop = NULL;
        } else {
            timeout.tv_sec  = (long)msec / 1000;
            timeout.tv_usec = (long)(msec % 1000 * 1000);
        }
#if defined(_WIN32)
        ctx.i = 0;
        ctx.readfds.reset(ctx.readset.size());
        for (auto fd : ctx.readset) {
            ctx.readfds.insert(fd);
        }
        ctx.writefds.reset(ctx.writeset.size());
        for (auto fd : ctx.writeset) {
            ctx.writefds.insert(fd);
        }
        int ok = ::select(0, ctx.readfds.ptr(), ctx.writefds.ptr(), ctx.writefds.ptr(), timeop);
#else
        ctx.i     = 1;
        ctx.maxfd = 0;
        FD_ZERO(&ctx.readfds);
        for (auto fd : ctx.readset) {
            FD_SET(fd, &ctx.readfds);
            ctx.maxfd = std::max(ctx.maxfd, fd);
        }
        FD_ZERO(&ctx.writefds);
        for (auto fd : ctx.writeset) {
            FD_SET(fd, &ctx.writefds);
            ctx.maxfd = std::max(ctx.maxfd, fd);
        }
        int ok;
        if (timeop == NULL) {
            do
                ok = ::select(ctx.maxfd + 1, &ctx.readfds, &ctx.writefds, NULL, NULL);
            while (ok == -1 && errno == EINTR);
        } else {
            ok = ::select(ctx.maxfd + 1, &ctx.readfds, &ctx.writefds, NULL, timeop);
            if (ok == -1 && errno == EINTR) {
                ok = 0;
            }
        }
#endif
        if (ok < 0) {
            lua::push_net_error(L, "select");
            return lua_error(L);
        }
        lua_getiuservalue(L, 1, 3);
        return 1;
    }
    static int close(lua_State* L) {
        auto& ctx = lua::checkudata<select_ctx>(L, 1);
        ctx.readset.clear();
        ctx.writeset.clear();
        return 0;
    }
    static net::fd_t tofd(lua_State* L, int idx) {
        switch (lua_type(L, idx)) {
        case LUA_TLIGHTUSERDATA:
            return lua::tolightud<net::fd_t>(L, idx);
        case LUA_TUSERDATA:
            return lua::toudata<net::fd_t>(L, idx);
        default:
            luaL_checktype(L, idx, LUA_TUSERDATA);
            std::unreachable();
        }
    }
    static int event_add(lua_State* L) {
        auto& ctx   = lua::checkudata<select_ctx>(L, 1);
        auto fd     = tofd(L, 2);
        auto events = luaL_checkinteger(L, 3);
        storeref(L, fd);
        if (events & SELECT_READ) {
            ctx.readset.insert(fd);
        } else {
            ctx.readset.erase(fd);
        }
        if (events & SELECT_WRITE) {
            ctx.writeset.insert(fd);
        } else {
            ctx.writeset.erase(fd);
        }
        lua_pushboolean(L, 1);
        return 1;
    }
    static int event_mod(lua_State* L) {
        auto& ctx   = lua::checkudata<select_ctx>(L, 1);
        auto fd     = tofd(L, 2);
        auto events = luaL_checkinteger(L, 3);
        if (events & SELECT_READ) {
            ctx.readset.insert(fd);
        } else {
            ctx.readset.erase(fd);
        }
        if (events & SELECT_WRITE) {
            ctx.writeset.insert(fd);
        } else {
            ctx.writeset.erase(fd);
        }
        lua_pushboolean(L, 1);
        return 1;
    }
    static int event_del(lua_State* L) {
        auto& ctx = lua::checkudata<select_ctx>(L, 1);
        auto fd   = tofd(L, 2);
        cleanref(L, fd);
        ctx.readset.erase(fd);
        ctx.writeset.erase(fd);
        lua_pushboolean(L, 1);
        return 1;
    }
    static void metatable(lua_State* L) {
        luaL_Reg lib[] = {
            { "wait", wait },
            { "close", close },
            { "event_add", event_add },
            { "event_mod", event_mod },
            { "event_del", event_del },
            { NULL, NULL },
        };
        luaL_newlibtable(L, lib);
        luaL_setfuncs(L, lib, 0);
        lua_setfield(L, -2, "__index");
        static luaL_Reg mt[] = {
            { "__close", close },
            { NULL, NULL }
        };
        luaL_setfuncs(L, mt, 0);
    }
    static int create(lua_State* L) {
        lua::newudata<select_ctx>(L);
        lua_newtable(L);
        lua_setiuservalue(L, -2, 1);
        lua_newtable(L);
        lua_setiuservalue(L, -2, 2);
        lua_pushvalue(L, -1);
        lua_pushcclosure(L, pairs_events, 1);
        lua_setiuservalue(L, -2, 3);
        lua_pushcclosure(L, empty_events, 0);
        lua_setiuservalue(L, -2, 4);
        return 1;
    }
    static int luaopen(lua_State* L) {
        struct luaL_Reg lib[] = {
            { "create", create },
            { NULL, NULL },
        };
        luaL_newlibtable(L, lib);
        luaL_setfuncs(L, lib, 0);
#define SETENUM(E)         \
    lua_pushinteger(L, E); \
    lua_setfield(L, -2, #E)

        SETENUM(SELECT_READ);
        SETENUM(SELECT_WRITE);
#undef SETENUM
        return 1;
    }
}

DEFINE_LUAOPEN(select)

namespace bee::lua {
    template <>
    struct udata<lua_select::select_ctx> {
        static inline int nupvalue   = 4;
        static inline auto metatable = bee::lua_select::metatable;
    };
}
