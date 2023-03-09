#include <binding/binding.h>
#include <bee/nonstd/semaphore.h>
#include <bee/nonstd/format.h>
#include <bee/thread/simplethread.h>
#include <bee/thread/spinlock.h>
#include <bee/thread/setname.h>
#include <bee/error.h>
#include <atomic>
#include <functional>
#include <map>
#include <mutex>
#include <queue>
#include <string>

extern "C" {
#include <lua-seri.h>
}

namespace bee::lua_thread {
    class channel {
    public:
        using value_type = void*;

        void push(value_type data) {
            do {
                std::unique_lock<spinlock> lk(mutex);
                queue.push(data);
            } while (0);
            sem.release();
        }
        bool pop(value_type& data) {
            std::unique_lock<spinlock> lk(mutex);
            if (queue.empty()) {
                return false;
            }
            data = queue.front();
            queue.pop();
            return true;
        }
        void blocked_pop(value_type& data) {
            for (;;) {
                if (pop(data)) {
                    return;
                }
                sem.acquire();
            }
        }
        template <class Rep, class Period>
        bool timed_pop(value_type& data, const std::chrono::duration<Rep, Period>& timeout) {
            auto now = std::chrono::steady_clock::now();
            if (pop(data)) {
                return true;
            }
            if (!sem.try_acquire_for(timeout)) {
                return false;
            }
            auto time = now + std::chrono::duration_cast<std::chrono::steady_clock::duration>(timeout);
            while (!pop(data)) {
                if (!sem.try_acquire_until(time)) {
                    return false;
                }
            }
            return true;
        }
    private:
        std::queue<value_type> queue;
        spinlock mutex;
        std::binary_semaphore sem = std::binary_semaphore(0);
    };

    using boxchannel = std::shared_ptr<channel>;

    struct rpc {
        std::binary_semaphore trigger = std::binary_semaphore(0);
        void* data = nullptr;
    };
}

namespace bee::lua {
    template <>
    struct udata<lua_thread::boxchannel> {
        static inline auto name = "bee::channel";
    };
    template <>
    struct udata<lua_thread::rpc> {
        static inline auto name = "bee::rpc";
    };
}

namespace bee::lua_thread {
    class channelmgr {
    public:
        channelmgr() {
            channels.emplace(std::make_pair("errlog", new channel));
        }
        bool create(std::string_view name) {
            std::unique_lock<spinlock> lk(mutex);
            std::string namestr {name};
            auto it = channels.find(namestr);
            if (it != channels.end()) {
                return false;
            }
            channels.emplace(std::make_pair(namestr, new channel));
            return true;
        }
        void clear() {
            std::unique_lock<spinlock> lk(mutex);
            auto                       it = channels.find("errlog");
            if (it != channels.end()) {
                auto errlog = it->second;
                channels.clear();
                channels.emplace(std::make_pair("errlog", std::move(errlog)));
            }
            else {
                channels.clear();
            }
        }
        boxchannel query(std::string_view name) {
            std::unique_lock<spinlock> lk(mutex);
            std::string namestr {name};
            auto it = channels.find(namestr);
            if (it != channels.end()) {
                return it->second;
            }
            return nullptr;
        }

    private:
        std::map<std::string, boxchannel> channels;
        spinlock mutex;
    };


    static channelmgr       g_channel;
    static std::atomic<int> g_thread_id = -1;
    static int       THREADID;

    static int lchannel_push(lua_State* L) {
        auto& bc = lua::checkudata<boxchannel>(L, 1);
        void* buffer = seri_pack(L, 1, NULL);
        bc->push(buffer);
        return 0;
    }

    static int lchannel_bpop(lua_State* L) {
        auto& bc = lua::checkudata<boxchannel>(L, 1);
        void* data;
        bc->blocked_pop(data);
        return seri_unpackptr(L, data);
    }

    static int lchannel_pop(lua_State* L) {
        auto& bc = lua::checkudata<boxchannel>(L, 1);
        void* data;
        lua_settop(L, 2);
        lua_Number sec = lua_tonumber(L, 2);
        if (sec == 0) {
            if (!bc->pop(data)) {
                lua_pushboolean(L, 0);
                return 1;
            }
        }
        else {
            if (!bc->timed_pop(data, std::chrono::duration<double>(sec))) {
                lua_pushboolean(L, 0);
                return 1;
            }
        }
        lua_pushboolean(L, 1);
        return 1 + seri_unpackptr(L, data);
    }

    static int lnewchannel(lua_State* L) {
        auto name = lua::checkstrview(L, 1);
        if (!g_channel.create(name)) {
            return luaL_error(L, "Duplicate channel '%s'", name.data());
        }
        return 0;
    }

    static void metatable(lua_State* L) {
        luaL_Reg lib[] = {
            {"push", lchannel_push},
            {"pop", lchannel_pop},
            {"bpop", lchannel_bpop},
            {NULL, NULL},
        };
        luaL_newlibtable(L, lib);
        luaL_setfuncs(L, lib, 0);
        lua_setfield(L, -2, "__index");
    }

    static int lchannel(lua_State* L) {
        auto name = lua::checkstrview(L, 1);
        boxchannel c = g_channel.query(name);
        if (!c) {
            return luaL_error(L, "Can't query channel '%s'", name.data());
        }
        lua::newudata<boxchannel>(L, metatable, c);
        return 1;
    }

    static int lsleep(lua_State* L) {
        lua_Number sec = luaL_checknumber(L, 1);
        thread_sleep((int)(sec * 1000));
        return 0;
    }

    struct thread_args {
        std::string source;
        int         id;
        void*       params;
    };

    static int gen_threadid() {
        for (;;) {
            int id = g_thread_id;
            if (g_thread_id.compare_exchange_weak(id, id + 1)) {
                id++;
                return id;
            }
        }
    }

    static int thread_luamain(lua_State* L) {
        lua_pushboolean(L, 1);
        lua_setfield(L, LUA_REGISTRYINDEX, "LUA_NOENV");
        luaL_openlibs(L);
        thread_args* args = lua::tolightud<thread_args*>(L, 1);
        lua_pushinteger(L, args->id);
        lua_rawsetp(L, LUA_REGISTRYINDEX, &THREADID);
        ::bee::lua::preload_module(L);
        lua_gc(L, LUA_GCGEN, 0, 0);
        if (luaL_loadbuffer(L, args->source.data(), args->source.size(), args->source.c_str()) != LUA_OK) {
            delete args;
            return lua_error(L);
        }
        void* params = args->params;
        delete args;
        int n = seri_unpackptr(L, params);
        lua_call(L, n, 0);
        return 0;
    }

    static int msghandler(lua_State* L) {
        const char* msg = lua_tostring(L, 1);
        if (msg == NULL) {
            if (luaL_callmeta(L, 1, "__tostring") && lua_type(L, -1) == LUA_TSTRING)
                return 1;
            else
                msg = lua_pushfstring(L, "(error object is a %s value)", luaL_typename(L, 1));
        }
        luaL_traceback(L, L, msg, 1);
        return 1;
    }

    static void thread_main(void* ud) {
        lua_State* L = luaL_newstate();
        lua_pushcfunction(L, msghandler);
        lua_pushcfunction(L, thread_luamain);
        lua_pushlightuserdata(L, ud);
        if (lua_pcall(L, 1, 0, 1) != LUA_OK) {
            boxchannel errlog = g_channel.query("errlog");
            if (errlog) {
                void* errmsg = seri_pack(L, lua_gettop(L)-1, NULL);
                errlog->push(errmsg);
            }
            else {
                std::println(stdout, "thread error : {}", lua_tostring(L, -1));
            }
        }
        lua_close(L);
    }

    static int lthread(lua_State* L) {
        auto source = lua::checkstrview(L, 1);
        void* params = seri_pack(L, 1, NULL);
        int id = gen_threadid();
        thread_args* args = new thread_args { std::string {source}, id, params };
        thread_handle handle = thread_create(thread_main, args);
        if (!handle) {
            delete args;
            lua_pushstring(L, make_syserror("thread_create").what());
            return lua_error(L);
        }
        lua_pushlightuserdata(L, handle);
        return 1;
    }

    static int lreset(lua_State* L) {
        lua_rawgetp(L, LUA_REGISTRYINDEX, &THREADID);
        int threadid = (int)lua_tointeger(L, -1);
        lua_pop(L, 1);
        if (threadid != 0) {
            return luaL_error(L, "reset must call from main thread");
        }
        g_channel.clear();
        g_thread_id = 0;
        return 0;
    }

    static int lwait(lua_State* L) {
        thread_handle th = lua::checklightud<thread_handle>(L, 1);
        thread_wait(th);
        return 0;
    }

    static int lsetname(lua_State* L) {
        thread_setname(luaL_checkstring(L, 1));
        return 0;
    }

    static void rpc_metatable(lua_State* L) {}

    static int lrpc_create(lua_State* L) {
        auto& r = lua::newudata<rpc>(L, rpc_metatable);
        lua_pushlightuserdata(L, &r);
        lua_rotate(L, 1, 1);
        return 2;
    }

    static int lrpc_wait(lua_State* L) {
        auto r = lua::checklightud<struct rpc *>(L, 1);
        r->trigger.acquire();
        return seri_unpackptr(L, r->data);
    }

    static int lrpc_return(lua_State* L) {
        auto r = lua::checklightud<struct rpc *>(L, 1);
        r->data = seri_pack(L, 1, NULL);
        r->trigger.release();
        return 0;
    }

    static void init_threadid(lua_State* L) {
        if (lua_rawgetp(L, LUA_REGISTRYINDEX, &THREADID) != LUA_TNIL) {
            return;
        }
        lua_pop(L, 1);

        lua_pushinteger(L, gen_threadid());
        lua_pushvalue(L, -1);
        lua_rawsetp(L, LUA_REGISTRYINDEX, &THREADID);
    }

    static int luaopen(lua_State* L) {
        luaL_Reg lib[] = {
            {"sleep", lsleep},
            {"thread", lthread},
            {"newchannel", lnewchannel},
            {"channel", lchannel},
            {"reset", lreset},
            {"wait", lwait},
            {"setname", lsetname},
            {"rpc_create", lrpc_create},
            {"rpc_wait", lrpc_wait},
            {"rpc_return", lrpc_return},
            {"preload_module", ::bee::lua::preload_module},
            {"id", NULL},
            {NULL, NULL},
        };
        luaL_newlibtable(L, lib);
        luaL_setfuncs(L, lib, 0);
        init_threadid(L);
        lua_setfield(L, -2, "id");
        return 1;
    }
}

DEFINE_LUAOPEN(thread)
