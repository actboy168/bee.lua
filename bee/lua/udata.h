#pragma once

#include <bee/reflection.h>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <lua.hpp>

namespace bee::lua {
    union lua_maxalign_t {
        LUAI_MAXALIGN;
    };
    constexpr inline size_t lua_maxalign = std::alignment_of_v<lua_maxalign_t>;
    template <typename T>
    T* udata_align(void* storage) {
        if constexpr (std::alignment_of_v<T> > lua_maxalign) {
            uintptr_t mask = (uintptr_t)(std::alignment_of_v<T> - 1);
            storage        = (void*)(((uintptr_t)storage + mask) & ~mask);
            return static_cast<T*>(storage);
        } else {
            return static_cast<T*>(storage);
        }
    }
    template <typename T>
    T* udata_new(lua_State* L, int nupvalue) {
        if constexpr (std::alignment_of_v<T> > lua_maxalign) {
            void* storage = lua_newuserdatauv(L, sizeof(T) + std::alignment_of_v<T>, nupvalue);
            return udata_align<T>(storage);
        } else {
            void* storage = lua_newuserdatauv(L, sizeof(T), nupvalue);
            return udata_align<T>(storage);
        }
    }
    template <typename T>
    T& toudata(lua_State* L, int arg) {
        return *udata_align<T>(lua_touserdata(L, arg));
    }
    template <typename T>
    T& checkudata(lua_State* L, int arg) {
        return *udata_align<T>(luaL_checkudata(L, arg, reflection::name_v<T>.data()));
    }
    template <typename T>
    int udata_destroy(lua_State* L) {
        toudata<T>(L, 1).~T();
        return 0;
    }

    template <typename T>
    struct udata {};
    template <typename T, typename = void>
    struct udata_has_nupvalue : std::false_type {};
    template <typename T>
    struct udata_has_nupvalue<T, std::void_t<decltype(udata<T>::nupvalue)>> : std::true_type {};

    template <typename T>
    void getmetatable(lua_State* L) {
        if (luaL_newmetatable(L, reflection::name_v<T>.data())) {
            if constexpr (!std::is_trivially_destructible<T>::value) {
                lua_pushcfunction(L, udata_destroy<T>);
                lua_setfield(L, -2, "__gc");
            }
            udata<T>::metatable(L);
        }
    }

    template <typename T, typename... Args>
    T& newudata(lua_State* L, Args&&... args) {
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
}
