#pragma once

#include <lua.hpp>
#include <vector>

#if defined(_WIN32)
#    include <Windows.h>
#else
#    include <dlfcn.h>
#endif

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

#if defined(_WIN32)
    inline lua_CFunction get_sym(const char* sym) {
        HMODULE lib = GetModuleHandleW(NULL);
        if (!lib) {
            return NULL;
        }
        return (lua_CFunction)GetProcAddress(lib, sym);
    }
#else
    inline lua_CFunction get_sym(const char* sym) {
        void* lib = dlopen(NULL, RTLD_NOW | RTLD_LOCAL);
        if (!lib) {
            return NULL;
        }
        return (lua_CFunction)dlsym(lib, sym);
    }
#endif

    inline int preload_module(lua_State* L) {
        luaL_getsubtable(L, LUA_REGISTRYINDEX, "_PRELOAD");
        for (const auto& m : usermodules()) {
            lua_pushcfunction(L, m.func);
            lua_setfield(L, -2, m.name);
        }
        lua_pop(L, 1);
        if (auto preload = get_sym("_bee_preload_module")) {
            preload(L);
        }
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
