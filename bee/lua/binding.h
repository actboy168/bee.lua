#pragma once

#include <bee/error.h>
#include <bee/nonstd/bit.h>
#include <bee/nonstd/to_underlying.h>
#include <bee/nonstd/unreachable.h>
#include <bee/utility/zstring_view.h>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <lua.hpp>
#include <string>
#if defined(_WIN32)
#    include <bee/platform/win/unicode.h>
#endif

namespace bee::lua {
#if defined(_WIN32)
    using string_type = std::wstring;
#else
    using string_type = std::string;
#endif

    inline zstring_view checkstrview(lua_State* L, int idx) {
        size_t len      = 0;
        const char* buf = luaL_checklstring(L, idx, &len);
        return { buf, len };
    }

    inline string_type checkstring(lua_State* L, int idx) {
        auto str = checkstrview(L, idx);
#if defined(_WIN32)
        return win::u2w(str);
#else
        return string_type { str.data(), str.size() };
#endif
    }

    template <typename T, typename I>
    constexpr bool checklimit(I i) {
        static_assert(std::is_integral_v<I>);
        static_assert(std::is_integral_v<T>);
        static_assert(sizeof(I) >= sizeof(T));
        if constexpr (sizeof(I) == sizeof(T)) {
            return true;
        }
        else if constexpr (std::numeric_limits<I>::is_signed == std::numeric_limits<T>::is_signed) {
            return i >= std::numeric_limits<T>::lowest() && i <= (std::numeric_limits<T>::max)();
        }
        else if constexpr (std::numeric_limits<I>::is_signed) {
            return static_cast<std::make_unsigned_t<I>>(i) >= std::numeric_limits<T>::lowest() && static_cast<std::make_unsigned_t<I>>(i) <= (std::numeric_limits<T>::max)();
        }
        else {
            return static_cast<std::make_signed_t<I>>(i) >= std::numeric_limits<T>::lowest() && static_cast<std::make_signed_t<I>>(i) <= (std::numeric_limits<T>::max)();
        }
    }

    template <typename T>
    T checkinteger(lua_State* L, int arg) {
        static_assert(std::is_trivial_v<T>);
        if constexpr (std::is_enum_v<T>) {
            using UT = std::underlying_type_t<T>;
            return static_cast<T>(checkinteger<UT>(L, arg));
        }
        else if constexpr (std::is_integral_v<T>) {
            lua_Integer r = luaL_checkinteger(L, arg);
            if constexpr (std::is_same_v<T, lua_Integer>) {
                return r;
            }
            else if constexpr (sizeof(T) >= sizeof(lua_Integer)) {
                return static_cast<T>(r);
            }
            else {
                if (checklimit<T>(r)) {
                    return static_cast<T>(r);
                }
                luaL_error(L, "bad argument '#%d' limit exceeded", arg);
                std::unreachable();
            }
        }
        else {
            return std::bit_cast<T>(checkinteger<lua_Integer>(L, arg));
        }
    }
    template <typename T, T def>
    T optinteger(lua_State* L, int arg) {
        static_assert(std::is_trivial_v<T>);
        if constexpr (std::is_enum_v<T>) {
            using UT = std::underlying_type_t<T>;
            return static_cast<T>(optinteger<UT, std::to_underlying(def)>(L, arg));
        }
        else if constexpr (std::is_integral_v<T>) {
            if constexpr (std::is_same_v<T, lua_Integer>) {
                return luaL_optinteger(L, arg, def);
            }
            else if constexpr (sizeof(T) == sizeof(lua_Integer)) {
                lua_Integer r = optinteger<lua_Integer, static_cast<lua_Integer>(def)>(L, arg);
                return static_cast<T>(r);
            }
            else if constexpr (sizeof(T) < sizeof(lua_Integer)) {
                lua_Integer r = optinteger<lua_Integer, static_cast<lua_Integer>(def)>(L, arg);
                if (checklimit<T>(r)) {
                    return static_cast<T>(r);
                }
                luaL_error(L, "bad argument '#%d' limit exceeded", arg);
                std::unreachable();
            }
            else {
                static_assert(checklimit<lua_Integer>(def));
                lua_Integer r = optinteger<lua_Integer, static_cast<lua_Integer>(def)>(L, arg);
                return static_cast<T>(r);
            }
        }
        else {
            // If std::bit_cast were not constexpr, it would fail here, so let it fail.
            return std::bit_cast<T>(optinteger<lua_Integer, std::bit_cast<lua_Integer>(def)>(L, arg));
        }
    }

    template <typename T>
    T tolightud(lua_State* L, int arg) {
        if constexpr (std::is_integral_v<T>) {
            uintptr_t r = std::bit_cast<uintptr_t>(tolightud<void*>(L, arg));
            if constexpr (std::is_same_v<T, uintptr_t>) {
                return r;
            }
            else if constexpr (sizeof(T) >= sizeof(uintptr_t)) {
                return static_cast<T>(r);
            }
            else {
                if (checklimit<T>(r)) {
                    return static_cast<T>(r);
                }
                luaL_error(L, "bad argument #%d limit exceeded", arg);
                std::unreachable();
            }
        }
        else if constexpr (std::is_same_v<T, void*>) {
            return lua_touserdata(L, arg);
        }
        else if constexpr (std::is_pointer_v<T>) {
            return static_cast<T>(tolightud<void*>(L, arg));
        }
        else {
            return std::bit_cast<T>(tolightud<void*>(L, arg));
        }
    }

    union lua_maxalign_t {
        LUAI_MAXALIGN;
    };
    constexpr inline size_t lua_maxalign = std::alignment_of_v<lua_maxalign_t>;

    template <typename T>
    constexpr T* udata_align(void* storage) {
        if constexpr (std::alignment_of_v<T> > lua_maxalign) {
            uintptr_t mask = (uintptr_t)(std::alignment_of_v<T> - 1);
            storage        = (void*)(((uintptr_t)storage + mask) & ~mask);
            return static_cast<T*>(storage);
        }
        else {
            return static_cast<T*>(storage);
        }
    }

    template <typename T>
    constexpr T* udata_new(lua_State* L, int nupvalue) {
        if constexpr (std::alignment_of_v<T> > lua_maxalign) {
            void* storage = lua_newuserdatauv(L, sizeof(T) + std::alignment_of_v<T>, nupvalue);
            return udata_align<T>(storage);
        }
        else {
            void* storage = lua_newuserdatauv(L, sizeof(T), nupvalue);
            return udata_align<T>(storage);
        }
    }

    template <typename T>
    T checklightud(lua_State* L, int arg) {
        luaL_checktype(L, arg, LUA_TLIGHTUSERDATA);
        return tolightud<T>(L, arg);
    }

    template <typename T>
    T& toudata(lua_State* L, int arg) {
        return *udata_align<T>(lua_touserdata(L, arg));
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
    void getmetatable(lua_State* L) {
        if (luaL_newmetatable(L, udata<T>::name)) {
            if constexpr (!std::is_trivially_destructible<T>::value) {
                lua_pushcfunction(L, destroyudata<T>);
                lua_setfield(L, -2, "__gc");
            }
            udata<T>::metatable(L);
        }
    }

    template <typename T, typename... Args>
    T& newudata(lua_State* L, Args&&... args) {
        static_assert(udata_has_name<T>::value);
        int nupvalue = 0;
        if constexpr (udata_has_nupvalue<T>::value) {
            nupvalue = udata<T>::nupvalue;
        }
        T* o = udata_new<T>(L, nupvalue);
        new (o) T(std::forward<Args>(args)...);
        getmetatable<T>(L);
        lua_setmetatable(L, -2);
        return *o;
    }

    template <typename T>
    T& checkudata(lua_State* L, int arg) {
        if constexpr (udata_has_name<T>::value) {
            return *udata_align<T>(luaL_checkudata(L, arg, udata<T>::name));
        }
        else {
            luaL_checktype(L, arg, LUA_TUSERDATA);
            return toudata<T>(L, arg);
        }
    }
}
