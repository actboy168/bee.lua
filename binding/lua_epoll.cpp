#include <bee/error.h>
#include <bee/lua/error.h>
#include <bee/net/bpoll.h>
#include <bee/nonstd/to_underlying.h>
#include <binding/binding.h>
#include <binding/luaref.h>

namespace bee::lua_epoll {
    static const net::bpoll_handle bpoll_invalid_handle = (net::bpoll_handle)-1;

    struct lua_epoll {
        net::bpoll_handle fd;
        size_t max_events;
        int i;
        int n;
        luaref ref;
        net::bpoll_event_t events[1];
    };

    struct lua_epoll *ep_get(lua_State *L) {
        return (struct lua_epoll *)luaL_checkudata(L, 1, "bee::epoll");
    }

    static net::bpoll_socket ep_tofd(lua_State *L, int idx) {
        return (net::bpoll_socket)(intptr_t)lua_touserdata(L, idx);
    }

    static int ep_events(lua_State *L) {
        struct lua_epoll *ep = (struct lua_epoll *)lua_touserdata(L, lua_upvalueindex(1));
        if (ep->i >= ep->n) {
            return 0;
        }
        const net::bpoll_event_t &ev = ep->events[ep->i];
        luaref_get(ep->ref, L, ev.data.u32);
        lua_pushinteger(L, std::to_underlying(ev.events));
        ep->i++;
        return 2;
    }

    static int ep_wait(lua_State *L) {
        struct lua_epoll *ep = ep_get(L);
        int timeout          = (int)luaL_optinteger(L, 2, -1);
        int n                = net::bpoll_wait(ep->fd, { ep->events, ep->max_events }, timeout);
        if (n == -1) {
            return lua::push_error(L, error::net_errmsg("epoll_wait"));
        }
        ep->i = 0;
        ep->n = n;
        lua_getiuservalue(L, 1, 2);
        return 1;
    }

    static bool ep_close_epoll(struct lua_epoll *ep) {
        if (!net::bpoll_close(ep->fd)) {
            return false;
        }
        ep->fd = bpoll_invalid_handle;
        return true;
    }

    static int ep_close(lua_State *L) {
        struct lua_epoll *ep = ep_get(L);
        if (!ep_close_epoll(ep)) {
            return lua::push_error(L, error::net_errmsg("epoll_close"));
        }
        lua_pushboolean(L, 1);
        return 1;
    }

    static int ep_mt_gc(lua_State *L) {
        struct lua_epoll *ep = ep_get(L);
        ep_close_epoll(ep);
        luaref_close(ep->ref);
        return 0;
    }

    static int ep_mt_close(lua_State *L) {
        struct lua_epoll *ep = ep_get(L);
        ep_close_epoll(ep);
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
        struct lua_epoll *ep = ep_get(L);
        if (ep->fd == bpoll_invalid_handle) {
            return lua::push_error(L, error::crt_errmsg("epoll_ctl", std::errc::bad_file_descriptor));
        }
        net::bpoll_socket fd = ep_tofd(L, 2);
        if (lua_isnoneornil(L, 4)) {
            lua_pushvalue(L, 2);
        }
        else {
            lua_pushvalue(L, 4);
        }
        int r = luaref_ref(ep->ref, L);
        if (r == LUA_NOREF) {
            return lua::push_error(L, "Too many events.");
        }
        net::bpoll_event_t ev;
        ev.events   = static_cast<net::bpoll_event>(luaL_checkinteger(L, 3));
        ev.data.u32 = r;
        if (!net::bpoll_ctl_add(ep->fd, fd, ev)) {
            luaref_unref(ep->ref, r);
            return lua::push_error(L, error::net_errmsg("epoll_ctl"));
        }
        storeref(L, r);
        lua_pushboolean(L, 1);
        return 1;
    }

    static int ep_event_mod(lua_State *L) {
        struct lua_epoll *ep = ep_get(L);
        net::bpoll_socket fd = ep_tofd(L, 2);
        int r                = findref(L);
        if (r == LUA_NOREF) {
            return lua::push_error(L, "event is not initialized.");
        }
        net::bpoll_event_t ev;
        ev.events   = static_cast<net::bpoll_event>(luaL_checkinteger(L, 3));
        ev.data.u32 = r;
        if (!net::bpoll_ctl_mod(ep->fd, fd, ev)) {
            return lua::push_error(L, error::net_errmsg("epoll_ctl"));
        }
        lua_pushboolean(L, 1);
        return 1;
    }

    static int ep_event_del(lua_State *L) {
        struct lua_epoll *ep = ep_get(L);
        net::bpoll_socket fd = ep_tofd(L, 2);
        if (!net::bpoll_ctl_del(ep->fd, fd)) {
            return lua::push_error(L, error::net_errmsg("epoll_ctl"));
        }
        int r = cleanref(L);
        if (r != LUA_NOREF) {
            luaref_unref(ep->ref, r);
        }
        lua_pushboolean(L, 1);
        return 1;
    }

    static int ep_create(lua_State *L) {
        lua_Integer max_events = luaL_checkinteger(L, 1);
        if (max_events <= 0) {
            return lua::push_error(L, "maxevents is less than or equal to zero.");
        }
        net::bpoll_handle epfd = net::bpoll_create();
        if (epfd == bpoll_invalid_handle) {
            return lua::push_error(L, error::net_errmsg("epoll_create"));
        }
        size_t sz            = sizeof(struct lua_epoll) + (max_events - 1) * sizeof(net::bpoll_event_t);
        struct lua_epoll *ep = (struct lua_epoll *)lua_newuserdatauv(L, sz, 2);
        lua_newtable(L);
        lua_setiuservalue(L, -2, 1);
        lua_pushvalue(L, -1);
        lua_pushcclosure(L, ep_events, 1);
        lua_setiuservalue(L, -2, 2);
        ep->fd         = epfd;
        ep->max_events = max_events;
        ep->ref        = luaref_init(L);
        ep->i          = 0;
        ep->n          = 0;
        if (luaL_newmetatable(L, "bee::epoll")) {
            luaL_Reg l[] = {
                { "wait", ep_wait },
                { "close", ep_close },
                { "event_add", ep_event_add },
                { "event_mod", ep_event_mod },
                { "event_del", ep_event_del },
                { "__gc", ep_mt_gc },
                { "__close", ep_mt_close },
                { "__index", NULL },
                { NULL, NULL },
            };
            luaL_setfuncs(L, l, 0);
            lua_pushvalue(L, -1);
            lua_setfield(L, -2, "__index");
        }
        lua_setmetatable(L, -2);
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

        return 1;
    }
}

DEFINE_LUAOPEN(epoll)
