#pragma once

#include <lua.hpp>
#include <string>
#if defined(_WIN32)
#include <bee/utility/unicode_win.h>
#endif

namespace bee::lua {
#if defined(_WIN32)
    typedef std::wstring string_type;
#else
    typedef std::string string_type;
#endif

    inline int push_error(lua_State* L, const std::exception& e)
    {
#if defined(_MSC_VER)
        lua_pushstring(L, a2u(e.what()).c_str());
#else
        lua_pushstring(L, e.what());
#endif
        return lua_error(L);
    }

    inline std::string_view to_strview(lua_State* L, int idx)
    {
        size_t len = 0;
        const char* buf = luaL_checklstring(L, idx, &len);
        return std::string_view(buf, len);
    }

    inline string_type checkstring(lua_State* L, int idx)
    {
        size_t len = 0;
        const char* buf = luaL_checklstring(L, idx, &len);
#if defined(_WIN32)
        return u2w(std::string_view(buf, len));
#else
        return std::string(buf, len);
#endif
    }

    inline string_type tostring(lua_State* L, int idx)
    {
        if (lua_isstring(L, idx)) {
            return checkstring(L, idx);
        }
        if (!luaL_callmeta(L, idx, "__tostring")) {
            luaL_error(L, "cannot be converted to string");
        }
        if (!lua_isstring(L, -1)) {
            luaL_error(L, "'__tostring' must return a string");
        }
        string_type res = checkstring(L, -1);
        lua_pop(L, 1);
        return res;
    }

    inline void push_string(lua_State* L, const string_type& str)
    {
#if defined(_WIN32) 
        std::string utf8 = w2u(str);
        lua_pushlstring(L, utf8.data(), utf8.size());
#else
        lua_pushlstring(L, str.data(), str.size());
#endif
    }
}

#define LUA_TRY     try {   
#define LUA_TRY_END } catch (const std::exception& e) { return lua::push_error(L, e); }

#if !defined(BEE_STATIC)
#    if defined(_WIN32)
#        define BEE_LUA_API extern "C" __declspec(dllexport)
#    else
#        define BEE_LUA_API extern "C" __attribute__((visibility("default")))
#    endif
#else
#    define BEE_LUA_API extern "C"
#endif

#define DEFINE_LUAOPEN(name) \
    BEE_LUA_API \
    int luaopen_bee_##name (lua_State* L) { \
        return bee::lua_##name ::luaopen(L); \
    }

#define newObject(L, name)      luaL_newmetatable((L), "bee::" name)
#define getObject(L, idx, name) luaL_checkudata((L), (idx), "bee::" name)

