#pragma once

#include <bee/nonstd/bit.h>
#include <bee/nonstd/print.h>
#include <bee/nonstd/to_underlying.h>
#include <bee/nonstd/unreachable.h>
#include <bee/reflection.h>
#include <bee/utility/zstring_view.h>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <lua.hpp>

namespace bee::lua {
    inline zstring_view checkstrview(lua_State* L, int idx) {
        size_t len      = 0;
        const char* buf = luaL_checklstring(L, idx, &len);
        return { buf, len };
    }

    template <typename T, typename I>
    constexpr bool checklimit(I i) {
        static_assert(std::is_integral_v<I>);
        static_assert(std::is_integral_v<T>);
        static_assert(sizeof(I) >= sizeof(T));
        if constexpr (sizeof(I) == sizeof(T)) {
            return true;
        } else if constexpr (std::numeric_limits<I>::is_signed == std::numeric_limits<T>::is_signed) {
            return i >= std::numeric_limits<T>::lowest() && i <= (std::numeric_limits<T>::max)();
        } else if constexpr (std::numeric_limits<I>::is_signed) {
            return static_cast<std::make_unsigned_t<I>>(i) >= std::numeric_limits<T>::lowest() && static_cast<std::make_unsigned_t<I>>(i) <= (std::numeric_limits<T>::max)();
        } else {
            return static_cast<std::make_signed_t<I>>(i) >= std::numeric_limits<T>::lowest() && static_cast<std::make_signed_t<I>>(i) <= (std::numeric_limits<T>::max)();
        }
    }

    template <typename T>
    T checkinteger(lua_State* L, int arg) {
        static_assert(std::is_trivial_v<T>);
        if constexpr (std::is_enum_v<T>) {
            using UT = std::underlying_type_t<T>;
            return static_cast<T>(checkinteger<UT>(L, arg));
        } else if constexpr (std::is_integral_v<T>) {
            lua_Integer r = luaL_checkinteger(L, arg);
            if constexpr (std::is_same_v<T, lua_Integer>) {
                return r;
            } else if constexpr (sizeof(T) >= sizeof(lua_Integer)) {
                return static_cast<T>(r);
            } else {
                if (checklimit<T>(r)) {
                    return static_cast<T>(r);
                }
                luaL_error(L, "bad argument '#%d' limit exceeded", arg);
                std::unreachable();
            }
        } else {
            return std::bit_cast<T>(checkinteger<lua_Integer>(L, arg));
        }
    }
    template <typename T, T def>
    T optinteger(lua_State* L, int arg) {
        static_assert(std::is_trivial_v<T>);
        if constexpr (std::is_enum_v<T>) {
            using UT = std::underlying_type_t<T>;
            return static_cast<T>(optinteger<UT, std::to_underlying(def)>(L, arg));
        } else if constexpr (std::is_integral_v<T>) {
            if constexpr (std::is_same_v<T, lua_Integer>) {
                return luaL_optinteger(L, arg, def);
            } else if constexpr (sizeof(T) == sizeof(lua_Integer)) {
                lua_Integer r = optinteger<lua_Integer, static_cast<lua_Integer>(def)>(L, arg);
                return static_cast<T>(r);
            } else if constexpr (sizeof(T) < sizeof(lua_Integer)) {
                lua_Integer r = optinteger<lua_Integer, static_cast<lua_Integer>(def)>(L, arg);
                if (checklimit<T>(r)) {
                    return static_cast<T>(r);
                }
                luaL_error(L, "bad argument '#%d' limit exceeded", arg);
                std::unreachable();
            } else {
                static_assert(checklimit<lua_Integer>(def));
                lua_Integer r = optinteger<lua_Integer, static_cast<lua_Integer>(def)>(L, arg);
                return static_cast<T>(r);
            }
        } else {
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
            } else if constexpr (sizeof(T) >= sizeof(uintptr_t)) {
                return static_cast<T>(r);
            } else {
                if (checklimit<T>(r)) {
                    return static_cast<T>(r);
                }
                luaL_error(L, "bad argument #%d limit exceeded", arg);
                std::unreachable();
            }
        } else if constexpr (std::is_same_v<T, void*>) {
            return lua_touserdata(L, arg);
        } else if constexpr (std::is_pointer_v<T>) {
            return static_cast<T>(tolightud<void*>(L, arg));
        } else {
            return std::bit_cast<T>(tolightud<void*>(L, arg));
        }
    }

    template <typename T>
    T checklightud(lua_State* L, int arg) {
        luaL_checktype(L, arg, LUA_TLIGHTUSERDATA);
        return tolightud<T>(L, arg);
    }
}
