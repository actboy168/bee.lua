#include <bee/error.h>
#include <bee/lua/binding.h>
#include <bee/lua/module.h>
#include <bee/thread/setname.h>
#include <bee/thread/simplethread.h>
#include <bee/thread/spinlock.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <queue>
#include <string>

extern "C" {
#include <3rd/lua-seri/lua-seri.h>
}

namespace bee::lua_thread {
    class errlog {
    public:
        void push(lua_State* L, int idx) noexcept {
            size_t len      = 0;
            const char* str = luaL_tolstring(L, idx, &len);
            std::string data(str, len);
            std::unique_lock<spinlock> _(mutex);
            queue.push(data);
        }
        int pop(lua_State* L) noexcept {
            std::unique_lock<spinlock> _(mutex);
            if (queue.empty()) {
                lua_pushboolean(L, 0);
                return 1;
            }
            auto& data = queue.front();
            lua_pushboolean(L, 1);
            lua_pushlstring(L, data.data(), data.size());
            queue.pop();
            return 2;
        }

    private:
        std::queue<std::string> queue;
        spinlock mutex;
    };

    static errlog g_errlog;
    static std::atomic<int> g_thread_id = -1;
    static int THREADID;

    static int lsleep(lua_State* L) {
        int msec = lua::checkinteger<int>(L, 1);
        thread_sleep(msec);
        return 0;
    }

    struct thread_args {
        std::string source;
        int id;
        void* params;
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
        lua::preload_module(L);
        lua_gc(L, LUA_GCGEN, 0, 0);
        if (luaL_loadbuffer(L, args->source.data(), args->source.size(), args->source.c_str()) != LUA_OK) {
            free(args->params);
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

    static void thread_main(void* ud) noexcept {
        lua_State* L = luaL_newstate();
        lua_pushcfunction(L, msghandler);
        lua_pushcfunction(L, thread_luamain);
        lua_pushlightuserdata(L, ud);
        if (lua_pcall(L, 1, 0, 1) != LUA_OK) {
            g_errlog.push(L, -1);
        }
        lua_close(L);
    }

    static int lcreate(lua_State* L) {
        auto source          = lua::checkstrview(L, 1);
        void* params         = seri_pack(L, 1, NULL);
        int id               = gen_threadid();
        thread_args* args    = new thread_args { std::string { source.data(), source.size() }, id, params };
        thread_handle handle = thread_create(thread_main, args);
        if (!handle) {
            free(params);
            delete args;
            lua_pushstring(L, error::sys_errmsg("thread_create").c_str());
            return lua_error(L);
        }
        lua_pushlightuserdata(L, handle);
        return 1;
    }

    static int lerrlog(lua_State* L) {
        return g_errlog.pop(L);
    }

    static int lwait(lua_State* L) {
        thread_handle th = lua::checklightud<thread_handle>(L, 1);
        thread_wait(th);
        return 0;
    }

    static int lsetname(lua_State* L) {
        thread_setname(lua::checkstrview(L, 1));
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
            { "sleep", lsleep },
            { "create", lcreate },
            { "errlog", lerrlog },
            { "wait", lwait },
            { "setname", lsetname },
            { "preload_module", lua::preload_module },
            { "id", NULL },
            { NULL, NULL },
        };
        luaL_newlibtable(L, lib);
        luaL_setfuncs(L, lib, 0);
        init_threadid(L);
        lua_setfield(L, -2, "id");
        return 1;
    }
}

DEFINE_LUAOPEN(thread)
