#include <bee/error.h>
#include <bee/lua/binding.h>
#include <bee/lua/module.h>
#include <bee/net/event.h>
#include <bee/net/socket.h>
#include <bee/thread/spinlock.h>

#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <string>

extern "C" {
#include <3rd/lua-seri/lua-seri.h>
}

namespace bee::lua_channel {
    class channel {
    public:
        using box = std::shared_ptr<channel>;

        bool init() noexcept {
            if (!ev.open()) {
                return false;
            }
            return true;
        }
        net::fd_t fd() const noexcept {
            return ev.fd();
        }
        void push(lua_State* L, int from) noexcept {
            void* data = seri_pack(L, from, NULL);
            std::unique_lock<spinlock> lk(mutex);
            queue.push(data);
            ev.set();
        }
        int pop(lua_State* L) noexcept {
            void* data;
            {
                std::unique_lock<spinlock> lk(mutex);
                if (queue.empty()) {
                    ev.clear();
                    lua_pushboolean(L, 0);
                    return 1;
                }
                data = queue.front();
                queue.pop();
            }
            lua_pushboolean(L, 1);
            return 1 + seri_unpackptr(L, data);
        }
        void clear() noexcept {
            std::unique_lock<spinlock> lk(mutex);
            for (;;) {
                if (queue.empty()) {
                    ev.clear();
                    return;
                }
                void* data = queue.front();
                free(data);
                queue.pop();
            }
        }

    private:
        std::queue<void*> queue;
        spinlock mutex;
        net::event ev;
    };

    class channelmgr {
    public:
        channel::box create(zstring_view name) noexcept {
            std::unique_lock<spinlock> lk(mutex);
            channel* c = new channel;
            if (!c->init()) {
                return nullptr;
            }
            std::string namestr { name.data(), name.size() };
            auto [r, ok] = channels.emplace(namestr, channel::box { c });
            if (!ok) {
                return nullptr;
            }
            return r->second;
        }
        void clear() noexcept {
            std::unique_lock<spinlock> lk(mutex);
            for (const auto& [_, c] : channels) {
                c->clear();
            }
            channels.clear();
        }
        channel::box query(zstring_view name) noexcept {
            std::unique_lock<spinlock> lk(mutex);
            std::string namestr { name.data(), name.size() };
            auto it = channels.find(namestr);
            if (it != channels.end()) {
                return it->second;
            }
            return nullptr;
        }

    private:
        std::map<std::string, channel::box> channels;
        spinlock mutex;
    };

    static channelmgr g_channel;

    static int lchannel_push(lua_State* L) {
        auto& bc = lua::checkudata<channel::box>(L, 1);
        bc->push(L, 1);
        return 0;
    }

    static int lchannel_pop(lua_State* L) {
        auto& bc = lua::checkudata<channel::box>(L, 1);
        return bc->pop(L);
    }

    static int lchannel_fd(lua_State* L) {
        auto& bc = lua::checkudata<channel::box>(L, 1);
        lua_pushlightuserdata(L, (void*)(intptr_t)bc->fd());
        return 1;
    }

    static void metatable(lua_State* L) {
        luaL_Reg lib[] = {
            { "push", lchannel_push },
            { "pop", lchannel_pop },
            { "fd", lchannel_fd },
            { NULL, NULL },
        };
        luaL_newlibtable(L, lib);
        luaL_setfuncs(L, lib, 0);
        lua_setfield(L, -2, "__index");
    }

    static int lcreate(lua_State* L) {
        auto name      = lua::checkstrview(L, 1);
        channel::box c = g_channel.create(name);
        if (!c) {
            return luaL_error(L, "Duplicate channel '%s'", name.data());
        }
        lua::newudata<channel::box>(L, c);
        return 1;
    }

    static int lquery(lua_State* L) {
        auto name      = lua::checkstrview(L, 1);
        channel::box c = g_channel.query(name);
        if (!c) {
            return luaL_error(L, "Can't query channel '%s'", name.data());
        }
        lua::newudata<channel::box>(L, c);
        return 1;
    }

    static int lreset(lua_State* L) {
        g_channel.clear();
        return 0;
    }

    static int luaopen(lua_State* L) {
        if (!net::socket::initialize()) {
            lua_pushstring(L, error::sys_errmsg("initialize").c_str());
            return lua_error(L);
        }
        luaL_Reg lib[] = {
            { "create", lcreate },
            { "query", lquery },
            { "reset", lreset },
            { NULL, NULL },
        };
        luaL_newlibtable(L, lib);
        luaL_setfuncs(L, lib, 0);
        return 1;
    }
}

DEFINE_LUAOPEN(channel)

namespace bee::lua {
    template <>
    struct udata<lua_channel::channel::box> {
        static inline auto name      = "bee::channel";
        static inline auto metatable = bee::lua_channel::metatable;
    };
}
