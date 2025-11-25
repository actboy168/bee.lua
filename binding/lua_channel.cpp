#include <3rd/lua-seri/lua-seri.h>
#include <bee/lua/binding.h>
#include <bee/lua/error.h>
#include <bee/lua/module.h>
#include <bee/lua/udata.h>
#include <bee/net/event.h>
#include <bee/net/socket.h>
#include <bee/thread/spinlock.h>

#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <string>

namespace bee::lua_channel {
    class channel {
    public:
        using box = std::shared_ptr<channel>;

        bool init() noexcept {
            return ev.open();
        }
        net::fd_t fd() const noexcept {
            return ev.fd();
        }
        void push(void* data) noexcept {
            std::unique_lock<spinlock> lk(mutex);
            queue.push(data);
            ev.set();
        }
        void* pop() noexcept {
            std::unique_lock<spinlock> lk(mutex);
            if (queue.empty()) {
                ev.clear();
                return nullptr;
            }
            void* data = queue.front();
            queue.pop();
            if (queue.empty()) {
                ev.clear();
            }
            return data;
        }
        void clear() noexcept {
            std::unique_lock<spinlock> lk(mutex);
            while (!queue.empty()) {
                void* data = queue.front();
                free(data);
                queue.pop();
            }
            ev.clear();
        }

    private:
        std::queue<void*> queue;
        spinlock mutex;
        net::event ev;
    };

    class channelmgr {
    public:
        channel::box create(std::string_view name) noexcept {
            std::unique_lock<spinlock> lk(mutex);
            channel* c = new channel;
            if (!c->init()) {
                delete c;
                return nullptr;
            }
            std::string namestr { name.data(), name.size() };
            auto [r, ok] = channels.emplace(namestr, channel::box { c });
            if (!ok) {
                return nullptr;
            }
            return r->second;
        }
        void destroy(std::string_view name) noexcept {
            std::unique_lock<spinlock> lk(mutex);
            std::string namestr { name.data(), name.size() };
            auto it = channels.find(namestr);
            if (it != channels.end()) {
                it->second->clear();
                channels.erase(it);
            }
        }
        channel::box query(std::string_view name) noexcept {
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
        auto& bc   = lua::checkudata<channel::box>(L, 1);
        void* data = seri_pack(L, 1, NULL);
        bc->push(data);
        return 0;
    }

    static int lchannel_pop(lua_State* L) {
        auto& bc   = lua::checkudata<channel::box>(L, 1);
        void* data = bc->pop();
        if (!data) {
            lua_pushboolean(L, 0);
            return 1;
        }
        lua_pushboolean(L, 1);
        return 1 + seri_unpackptr(L, data);
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

    static int ldestroy(lua_State* L) {
        auto name = lua::checkstrview(L, 1);
        g_channel.destroy(name);
        return 0;
    }

    static int lquery(lua_State* L) {
        auto name      = lua::checkstrview(L, 1);
        channel::box c = g_channel.query(name);
        if (!c) {
            luaL_pushfail(L);
            lua_pushfstring(L, "Can't query channel '%s'", name.data());
            return 2;
        }
        lua::newudata<channel::box>(L, c);
        return 1;
    }

    static int luaopen(lua_State* L) {
        if (!net::socket::initialize()) {
            lua::push_sys_error(L, "initialize");
            return lua_error(L);
        }
        luaL_Reg lib[] = {
            { "create", lcreate },
            { "destroy", ldestroy },
            { "query", lquery },
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
        static inline auto metatable = bee::lua_channel::metatable;
    };
}
