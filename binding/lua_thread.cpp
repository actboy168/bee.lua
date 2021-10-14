#include <atomic>
#include <bee/lua/binding.h>
#include <bee/thread/lockqueue.h>
#include <bee/thread/semaphore.h>
#include <bee/thread/simplethread.h>
#include <bee/thread/spinlock.h>
#include <bee/error.h>
#include <functional>
#include <lua.hpp>
#include <map>
#include <queue>
#include <string>

extern "C" {
#include <lua-seri.h>
}

namespace bee::lua_thread {
    class channel : public lockqueue<void*> {
    public:
        typedef lockqueue<void*> mybase;

        void push(void* data) {
            mybase::push(data);
            sem.release();
        }
        void blocked_pop(void*& data) {
            for (;;) {
                if (mybase::pop(data)) {
                    return;
                }
                sem.acquire();
            }
        }
        template <class Rep, class Period>
        bool timed_pop(void*& data, const std::chrono::duration<Rep, Period>& timeout) {
            auto now = std::chrono::steady_clock::now();
            if (mybase::pop(data)) {
                return true;
            }
            if (!sem.try_acquire_for(timeout)) {
                return false;
            }
            while (!mybase::pop(data)) {
                if (!sem.try_acquire_until(now + timeout)) {
                    return false;
                }
            }
            return true;
        }

    private:
        binary_semaphore sem;
    };

    class channelmgr {
    public:
        channelmgr() {
            channels.emplace(std::make_pair("errlog", new channel));
        }
        bool create(const std::string& name) {
            std::unique_lock<spinlock> lk(mutex);
            auto                       it = channels.find(name);
            if (it != channels.end()) {
                return false;
            }
            channels.emplace(std::make_pair(name, new channel));
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
        std::shared_ptr<channel> query(const std::string& name) {
            std::unique_lock<spinlock> lk(mutex);
            auto                       it = channels.find(name);
            if (it != channels.end()) {
                return it->second;
            }
            return nullptr;
        }

    private:
        std::map<std::string, std::shared_ptr<channel>> channels;
        spinlock                                        mutex;
    };

    using boxchannel = std::shared_ptr<channel>;

    static channelmgr       g_channel;
    static std::atomic<int> g_thread_id = -1;
    static int       THREADID;

    static std::string checkstring(lua_State* L, int idx) {
        size_t      len = 0;
        const char* buf = luaL_checklstring(L, idx, &len);
        return std::string(buf, len);
    }

    static int lchannel_push(lua_State* L) {
        boxchannel& bc = *(boxchannel*)getObject(L, 1, "channel");
        void*       buffer = seri_pack(L, 1, NULL);
        bc->push(buffer);
        return 0;
    }

    static int lchannel_bpop(lua_State* L) {
        boxchannel& bc = *(boxchannel*)getObject(L, 1, "channel");
        void*       data;
        bc->blocked_pop(data);
        return seri_unpackptr(L, data);
    }

    static int lchannel_pop(lua_State* L) {
        boxchannel& bc = *(boxchannel*)getObject(L, 1, "channel");
        void*       data;
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

    struct rpc {
        binary_semaphore trigger;
        void* data = nullptr;
    };

    static int lchannel_call(lua_State* L) {
        boxchannel& bc = *(boxchannel*)getObject(L, 1, "channel");
        struct rpc* r = (struct rpc*)lua_newuserdatauv(L, sizeof(*r), 0);
        new (r) rpc;
        lua_pushlightuserdata(L, r);
        lua_rotate(L, 2, 2);
        void * buffer = seri_pack(L, 2, NULL);
        bc->push(buffer);
        r->trigger.acquire();
        return seri_unpackptr(L, r->data);
    }

    static int lchannel_ret(lua_State* L) {
        luaL_checktype(L, 2, LUA_TLIGHTUSERDATA);
        struct rpc *r = (struct rpc *)lua_touserdata(L, 2);
        r->data = seri_pack(L, 2, NULL);
        r->trigger.release();
        return 0;
    }

    static int lchannel_gc(lua_State* L) {
        boxchannel* bc = (boxchannel*)getObject(L, 1, "channel");
        bc->~boxchannel();
        return 0;
    }

    static int lnewchannel(lua_State* L) {
        std::string name = checkstring(L, 1);
        if (!g_channel.create(name)) {
            return luaL_error(L, "Duplicate channel '%s'", name.c_str());
        }
        return 0;
    }

    static int lchannel(lua_State* L) {
        std::string              name = checkstring(L, 1);
        std::shared_ptr<channel> c = g_channel.query(name);
        if (!c) {
            return luaL_error(L, "Can't query channel '%s'", name.c_str());
        }

        boxchannel* bc = (boxchannel*)lua_newuserdatauv(L, sizeof(boxchannel), 0);
        new (bc) boxchannel(c);
        if (newObject(L, "channel")) {
            luaL_Reg mt[] = {
                {"push", lchannel_push},
                {"pop", lchannel_pop},
                {"bpop", lchannel_bpop},
                {"call", lchannel_call},
                {"ret", lchannel_ret},
                {"__gc", lchannel_gc},
                {NULL, NULL},
            };
            luaL_setfuncs(L, mt, 0);
            lua_pushvalue(L, -1);
            lua_setfield(L, -2, "__index");
        }
        lua_setmetatable(L, -2);
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
        void*        ud = lua_touserdata(L, 1);
        thread_args* args = (thread_args*)ud;
        lua_pushinteger(L, args->id);
        lua_rawsetp(L, LUA_REGISTRYINDEX, &THREADID);
        ::bee::lua::preload_module(L);

#if LUA_VERSION_NUM >= 504
        lua_gc(L, LUA_GCGEN, 0, 0);
#endif
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
            std::shared_ptr<channel> errlog = g_channel.query("errlog");
            if (errlog) {
                size_t      sz;
                const char* str = lua_tolstring(L, -1, &sz);
                void*       errmsg = seri_packstring(str, (int)sz);
                errlog->push(errmsg);
            }
            else {
                printf("thread error : %s", lua_tostring(L, -1));
            }
        }
        lua_close(L);
    }

    static int lthread(lua_State* L) {
        std::string source = checkstring(L, 1);
        void*       params = seri_pack(L, 1, NULL);
        LUA_TRY;
        int id = gen_threadid();
        thread_args* args = new thread_args { std::move(source), id, params };
        thread_handle handle = thread_create(thread_main, args);
        if (!handle) {
            auto error = make_syserror("thread_create");
            delete args;
            return luaL_error(L, "%s (%d)", error.what(), error.code().value());
        }
        lua_pushlightuserdata(L, handle);
        return 1;
        LUA_TRY_END;
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
        luaL_checktype(L, 1, LUA_TLIGHTUSERDATA);
        thread_handle th = (thread_handle)lua_touserdata(L, 1);
        thread_wait(th);
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
            {NULL, NULL},
        };
        lua_newtable(L);
        luaL_setfuncs(L, lib, 0);
        init_threadid(L);
        lua_setfield(L, -2, "id");
        return 1;
    }
}

DEFINE_LUAOPEN(thread)
