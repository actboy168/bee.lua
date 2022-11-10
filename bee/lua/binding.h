#pragma once

#include <lua.hpp>
#include <map>
#include <string>
#include <bee/error.h>
#if defined(_WIN32)
#include <bee/utility/unicode_win.h>
#endif

namespace bee::lua {
#if defined(_WIN32)
    typedef std::wstring string_type;
#else
    typedef std::string string_type;
#endif

    inline std::string_view to_strview(lua_State* L, int idx) {
        size_t len = 0;
        const char* buf = luaL_checklstring(L, idx, &len);
        return std::string_view(buf, len);
    }

    inline string_type to_string(lua_State* L, int idx) {
        auto str = to_strview(L, idx);
#if defined(_WIN32)
        return u2w(str);
#else
        return string_type { str };
#endif
    }

    inline void push_string(lua_State* L, const string_type& str) {
#if defined(_WIN32) 
        std::string utf8 = w2u(str);
        lua_pushlstring(L, utf8.data(), utf8.size());
#else
        lua_pushlstring(L, str.data(), str.size());
#endif
    }

    inline void push_errormesg(lua_State* L, const char* msg, const std::error_code& ec) {
        lua_pushfstring(L, "%s: %s (%d)", msg, error_message(ec).c_str(), ec.value());
    }

    inline void push_errormesg(lua_State* L, const char* msg, const std::system_error& err) {
        push_errormesg(L, msg, err.code());
    }

    template <typename T>
    struct global { static inline T v = T(); };
    using usermodules = global<std::map<std::string, lua_CFunction>>;

    struct callfunc {
        template <typename F, typename ...Args>
        callfunc(F f, Args... args) {
            f(std::forward<Args>(args)...);
        }
    };

    inline bool register_module(const char* name, lua_CFunction func) {
        return usermodules::v.insert(std::pair(name, func)).second;
    }

    inline int preload_module(lua_State* L) {
        luaL_getsubtable(L, LUA_REGISTRYINDEX, "_PRELOAD");
        for (auto const& m : usermodules::v) {
            lua_pushcfunction(L, m.second);
            lua_setfield(L, -2, m.first.c_str());
        }
        lua_pop(L, 1);
        return 0;
    }
}

#if !defined(BEE_STATIC)
#    if defined(_WIN32)
#        define BEE_LUA_API extern "C" __declspec(dllexport)
#    else
#        define BEE_LUA_API extern "C" __attribute__((visibility("default")))
#    endif
#else
#    define BEE_LUA_API extern "C"
#endif

#define DEFINE_LUAOPEN(name)                 \
    BEE_LUA_API                              \
    int luaopen_bee_##name(lua_State* L) {   \
        return bee::lua_##name ::luaopen(L); \
    }                                        \
    static ::bee::lua::callfunc _init_##name(::bee::lua::register_module, "bee."#name, luaopen_bee_##name);


#define newObject(L, name)      luaL_newmetatable((L), "bee::" name)
#define getObject(L, idx, name) luaL_checkudata((L), (idx), "bee::" name)
