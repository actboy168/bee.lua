#pragma once

#include <lua.hpp>
#include <vector>

namespace bee::lua {
    struct callfunc {
        template <typename F, typename... Args>
        callfunc(F f, Args... args) {
            f(std::forward<Args>(args)...);
        }
    };

    inline auto& usermodules() {
        static std::vector<luaL_Reg> v;
        return v;
    }

    inline void register_module(const char* name, lua_CFunction func) {
        usermodules().emplace_back(luaL_Reg { name, func });
    }

    inline int preload_module(lua_State* L) {
        luaL_getsubtable(L, LUA_REGISTRYINDEX, "_PRELOAD");
        for (const auto& m : usermodules()) {
            lua_pushcfunction(L, m.func);
            lua_setfield(L, -2, m.name);
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
    static ::bee::lua::callfunc _init_##name(::bee::lua::register_module, "bee." #name, luaopen_bee_##name);
