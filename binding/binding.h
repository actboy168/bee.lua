#pragma once

#include <lua.hpp>
#include <map>
#include <string>
#include <limits>
#include <stdint.h>
#include <bee/error.h>
#include <bee/nonstd/unreachable.h>
#if defined(_WIN32)
#include <bee/platform/win/unicode.h>
#endif

namespace bee::lua {
#if defined(_WIN32)
    using string_type = std::wstring;
#else
    using string_type = std::string;
#endif

    inline std::string_view checkstrview(lua_State* L, int idx) {
        size_t len = 0;
        const char* buf = luaL_checklstring(L, idx, &len);
        return std::string_view(buf, len);
    }

    inline string_type checkstring(lua_State* L, int idx) {
        auto str = checkstrview(L, idx);
#if defined(_WIN32)
        return win::u2w(str);
#else
        return string_type { str };
#endif
    }

    template <typename T>
    T checkinteger(lua_State* L, int arg, const char* symbol) {
        if constexpr (std::is_enum<T>::value) {
            using UT = typename std::underlying_type<T>::type;
            return (T)checkinteger<UT>(L, arg, symbol);
        }
        else if constexpr (sizeof(T) == sizeof(lua_Integer)) {
            return (T)luaL_checkinteger(L, arg);
        }
        else {
            static_assert(sizeof(T) < sizeof(lua_Integer));
            lua_Integer r = luaL_checkinteger(L, arg);
            if (r >= std::numeric_limits<T>::lowest() && r <= (std::numeric_limits<T>::max)()) {
                return (T)r;
            }
            luaL_error(L, "bad argument '%s' limit exceeded", symbol);
            std::unreachable();
        }
    }
    template <typename T>
    T optinteger(lua_State* L, int arg, T def, const char* symbol) {
        if constexpr (std::is_enum<T>::value) {
            using UT = typename std::underlying_type<T>::type;
            return (T)optinteger<UT>(L, arg, (UT)def, symbol);
        }
        else if constexpr (sizeof(T) == sizeof(lua_Integer)) {
            return (T)luaL_optinteger(L, arg, (lua_Integer)def);
        }
        else {
            static_assert(sizeof(T) < sizeof(lua_Integer));
            lua_Integer r = luaL_optinteger(L, arg, (lua_Integer)def);
            if (r >= std::numeric_limits<T>::lowest() && r <= (std::numeric_limits<T>::max)()) {
                return (T)r;
            }
            luaL_error(L, "bad argument '%s' limit exceeded", symbol);
            std::unreachable();
        }
    }

    template <typename T>
    T checklightud(lua_State* L, int arg) {
        static_assert(sizeof(T) <= sizeof(intptr_t));
        luaL_checktype(L, arg, LUA_TLIGHTUSERDATA);
        return (T)(intptr_t)lua_touserdata(L, arg);
    }

    template <typename T>
    T& toudata(lua_State* L, int arg) {
        return *(T*)lua_touserdata(L, arg);
    }

    template <typename T>
    struct udata {};
    template <typename T, typename = void>
    struct udata_has_name : std::false_type {};
    template <typename T>
    struct udata_has_name<T, std::void_t<decltype(udata<T>::name)>> : std::true_type {};
    template <typename T, typename = void>
    struct udata_has_nupvalue : std::false_type {};
    template <typename T>
    struct udata_has_nupvalue<T, std::void_t<decltype(udata<T>::nupvalue)>> : std::true_type {};

    template <typename T>
    int destroyudata(lua_State* L) {
        toudata<T>(L, 1).~T();
        return 0;
    }

    template <typename T>
    T& newudata(lua_State* L) {
        static_assert(!udata_has_name<T>::value);
        static_assert(std::is_trivial<T>::value);
        int nupvalue = 0;
        if constexpr (udata_has_nupvalue<T>::value) {
            nupvalue = udata<T>::nupvalue;
        }
        T* o = (T*)lua_newuserdatauv(L, sizeof(T), nupvalue);
        return *o;
    }

    template <typename T, typename...Args>
    T& newudata(lua_State* L, void (*init_metatable)(lua_State*), Args&&...args) {
        static_assert(udata_has_name<T>::value);
        int nupvalue = 0;
        if constexpr (udata_has_nupvalue<T>::value) {
            nupvalue = udata<T>::nupvalue;
        }
        T* o = (T*)lua_newuserdatauv(L, sizeof(T), nupvalue);
        new (o) T(std::forward<Args>(args)...);
        if (luaL_newmetatable(L, udata<T>::name)) {
            init_metatable(L);
            if constexpr (!std::is_trivially_destructible<T>::value) {
                lua_pushcfunction(L, destroyudata<T>);
                lua_setfield(L, -2, "__gc");
            }
        }
        lua_setmetatable(L, -2);
        return *o;
    }

    template <typename T>
    T& checkudata(lua_State* L, int ud) {
        if constexpr (udata_has_name<T>::value) {
            return *(T*)luaL_checkudata(L, ud, udata<T>::name);
        }
        else {
            return toudata<T>(L, ud);
        }
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
