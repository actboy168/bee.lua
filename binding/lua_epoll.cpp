#include <bee/lua/binding.h>
#include <bee/lua/error.h>
#include <bee/lua/luaref.h>
#include <bee/lua/module.h>
#include <bee/lua/udata.h>
#include <bee/net/bpoll.h>
#include <bee/nonstd/to_underlying.h>
#include <bee/utility/dynarray.h>

namespace bee::lua_epoll {
    struct lua_epoll {
        net::bpoll_handle fd;
        int i = 0;
        int n = 0;
        luaref ref;
        dynarray<net::bpoll_event_t> events;
        lua_epoll(lua_State *L, net::bpoll_handle epfd, size_t max_events)
            : fd(epfd)
            , ref(luaref_init(L))
            , events(max_events) {
        }
        ~lua_epoll() {
            close();
            luaref_close(ref);
        }
        bool close() {
            if (!net::bpoll_close(fd)) {
                return false;
            }
            fd = net::invalid_bpoll_handle;
            return true;
        }
    };

    static net::fd_t ep_tofd(lua_State *L, int idx) {
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

    static int ep_events(lua_State *L) {
        auto &ep = *(lua_epoll *)lua_touserdata(L, lua_upvalueindex(1));
        if (ep.i >= ep.n) {
            return 0;
        }
        const auto &ev = ep.events[ep.i];
        luaref_get(ep.ref, L, ev.data.u32);
        lua_pushinteger(L, static_cast<uint32_t>(ev.events));
        ep.i++;
        return 2;
    }

    static int ep_wait(lua_State *L) {
        auto &ep = lua::checkudata<lua_epoll>(L, 1);
        if (ep.fd == net::invalid_bpoll_handle) {
            return lua::return_error(L, "bad file descriptor");
        }
        int timeout = lua::optinteger<int, -1>(L, 2);
        int n       = net::bpoll_wait(ep.fd, ep.events, timeout);
        if (n == -1) {
            return lua::return_net_error(L, "epoll_wait");
        }
        ep.i = 0;
        ep.n = n;
        lua_getiuservalue(L, 1, 2);
        return 1;
    }

    static int ep_close(lua_State *L) {
        auto &ep = lua::checkudata<lua_epoll>(L, 1);
        if (!ep.close()) {
            return lua::return_net_error(L, "epoll_close");
        }
        lua_pushboolean(L, 1);
        return 1;
    }

    static int ep_mt_close(lua_State *L) {
        auto &ep = lua::checkudata<lua_epoll>(L, 1);
        ep.close();
        return 0;
    }

    static void storeref(lua_State *L, int r) {
        lua_getiuservalue(L, 1, 1);
        lua_pushvalue(L, 2);
        lua_pushinteger(L, r);
        lua_rawset(L, -3);
        lua_pop(L, 1);
    }

    static int cleanref(lua_State *L) {
        lua_getiuservalue(L, 1, 1);
        lua_pushvalue(L, 2);
        if (LUA_TNUMBER != lua_rawget(L, -2)) {
            lua_pop(L, 2);
            return LUA_NOREF;
        }
        int r = (int)lua_tointeger(L, -1);
        lua_pop(L, 1);
        lua_pushvalue(L, 2);
        lua_pushnil(L);
        lua_rawset(L, -3);
        lua_pop(L, 1);
        return r;
    }

    static int findref(lua_State *L) {
        lua_getiuservalue(L, 1, 1);
        lua_pushvalue(L, 2);
        if (LUA_TNUMBER != lua_rawget(L, -2)) {
            lua_pop(L, 2);
            return LUA_NOREF;
        }
        int r = (int)lua_tointeger(L, -1);
        lua_pop(L, 2);
        return r;
    }

    static int ep_event_add(lua_State *L) {
        auto &ep = lua::checkudata<lua_epoll>(L, 1);
        if (ep.fd == net::invalid_bpoll_handle) {
            return lua::return_error(L, "bad file descriptor");
        }
        net::fd_t fd = ep_tofd(L, 2);
        if (lua_isnoneornil(L, 4)) {
            lua_pushvalue(L, 2);
        } else {
            lua_pushvalue(L, 4);
        }
        int r = luaref_ref(ep.ref, L);
        if (r == LUA_NOREF) {
            return lua::return_error(L, "Too many events.");
        }
        net::bpoll_event_t ev;
        ev.events   = static_cast<decltype(ev.events)>(luaL_checkinteger(L, 3));
        ev.data.u32 = r;
        if (!net::bpoll_ctl_add(ep.fd, fd, ev)) {
            luaref_unref(ep.ref, r);
            return lua::return_net_error(L, "epoll_ctl");
        }
        storeref(L, r);
        lua_pushboolean(L, 1);
        return 1;
    }

    static int ep_event_mod(lua_State *L) {
        auto &ep = lua::checkudata<lua_epoll>(L, 1);
        if (ep.fd == net::invalid_bpoll_handle) {
            return lua::return_error(L, "bad file descriptor");
        }
        net::fd_t fd = ep_tofd(L, 2);
        int r        = findref(L);
        if (r == LUA_NOREF) {
            return lua::return_error(L, "event is not initialized.");
        }
        net::bpoll_event_t ev;
        ev.events   = static_cast<decltype(ev.events)>(luaL_checkinteger(L, 3));
        ev.data.u32 = r;
        if (!net::bpoll_ctl_mod(ep.fd, fd, ev)) {
            return lua::return_net_error(L, "epoll_ctl");
        }
        if (lua_gettop(L) >= 4) {
            lua_pushvalue(L, 4);
            luaref_set(ep.ref, L, r);
        }
        lua_pushboolean(L, 1);
        return 1;
    }

    static int ep_event_del(lua_State *L) {
        auto &ep = lua::checkudata<lua_epoll>(L, 1);
        if (ep.fd == net::invalid_bpoll_handle) {
            return lua::return_error(L, "bad file descriptor");
        }
        net::fd_t fd = ep_tofd(L, 2);
        if (!net::bpoll_ctl_del(ep.fd, fd)) {
            return lua::return_net_error(L, "epoll_ctl");
        }
        int r = cleanref(L);
        if (r != LUA_NOREF) {
            luaref_unref(ep.ref, r);
        }
        lua_pushboolean(L, 1);
        return 1;
    }

    static void metatable(lua_State *L) {
        static luaL_Reg lib[] = {
            { "wait", ep_wait },
            { "close", ep_close },
            { "event_add", ep_event_add },
            { "event_mod", ep_event_mod },
            { "event_del", ep_event_del },
            { NULL, NULL }
        };
        luaL_newlibtable(L, lib);
        luaL_setfuncs(L, lib, 0);
        lua_setfield(L, -2, "__index");
        static luaL_Reg mt[] = {
            { "__close", ep_mt_close },
            { NULL, NULL }
        };
        luaL_setfuncs(L, mt, 0);
    }

    static int ep_create(lua_State *L) {
        lua_Integer max_events = luaL_checkinteger(L, 1);
        if (max_events <= 0) {
            return lua::return_error(L, "maxevents is less than or equal to zero.");
        }
        net::bpoll_handle epfd = net::bpoll_create();
        if (epfd == (net::bpoll_handle)-1) {
            return lua::return_net_error(L, "epoll_create");
        }
        lua::newudata<lua_epoll>(L, L, epfd, (size_t)max_events);
        lua_newtable(L);
        lua_setiuservalue(L, -2, 1);
        lua_pushvalue(L, -1);
        lua_pushcclosure(L, ep_events, 1);
        lua_setiuservalue(L, -2, 2);
        return 1;
    }

    static int luaopen(lua_State *L) {
        struct luaL_Reg l[] = {
            { "create", ep_create },
            { NULL, NULL },
        };
        luaL_newlib(L, l);

#define SETENUM(E, V)                          \
    lua_pushinteger(L, std::to_underlying(V)); \
    lua_setfield(L, -2, #E)

        SETENUM(EPOLLIN, net::bpoll_event::in);
        SETENUM(EPOLLPRI, net::bpoll_event::pri);
        SETENUM(EPOLLOUT, net::bpoll_event::out);
        SETENUM(EPOLLERR, net::bpoll_event::err);
        SETENUM(EPOLLHUP, net::bpoll_event::hup);
        SETENUM(EPOLLRDNORM, net::bpoll_event::rdnorm);
        SETENUM(EPOLLRDBAND, net::bpoll_event::rdband);
        SETENUM(EPOLLWRNORM, net::bpoll_event::wrnorm);
        SETENUM(EPOLLWRBAND, net::bpoll_event::wrand);
        SETENUM(EPOLLMSG, net::bpoll_event::msg);
        SETENUM(EPOLLRDHUP, net::bpoll_event::rdhup);
        SETENUM(EPOLLONESHOT, net::bpoll_event::oneshot);
#undef SETENUM
        return 1;
    }
}

DEFINE_LUAOPEN(epoll)

namespace bee::lua {
    template <>
    struct udata<lua_epoll::lua_epoll> {
        static inline int nupvalue   = 2;
        static inline auto metatable = bee::lua_epoll::metatable;
    };
}
