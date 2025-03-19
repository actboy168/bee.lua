#include <bee/lua/module.h>

#include <cstddef>
#include <lua.hpp>

static const luaL_Reg loadedlibs[] = {
    { LUA_GNAME, luaopen_base },
    { LUA_LOADLIBNAME, luaopen_package },
    { LUA_COLIBNAME, luaopen_coroutine },
    { LUA_TABLIBNAME, luaopen_table },
    { LUA_IOLIBNAME, luaopen_io },
    { LUA_OSLIBNAME, luaopen_os },
    { LUA_STRLIBNAME, luaopen_string },
    { LUA_MATHLIBNAME, luaopen_math },
    { LUA_UTF8LIBNAME, luaopen_utf8 },
    { LUA_DBLIBNAME, luaopen_debug },
    { NULL, NULL }
};

#if LUA_VERSION_NUM >= 505

extern "C" void luaL_openselectedlibs(lua_State *L, int load, int preload) {
    int mask;
    const luaL_Reg *lib;
    luaL_getsubtable(L, LUA_REGISTRYINDEX, LUA_PRELOAD_TABLE);
    for (lib = loadedlibs, mask = 1; lib->name != NULL; lib++, mask <<= 1) {
        if (load & mask) {
            luaL_requiref(L, lib->name, lib->func, 1);
            lua_pop(L, 1);
        } else if (preload & mask) {
            lua_pushcfunction(L, lib->func);
            lua_setfield(L, -2, lib->name);
        }
    }
    lua_pop(L, 1);
    ::bee::lua::preload_module(L);
}

#else

extern "C" void luaL_openlibs(lua_State *L) {
    const luaL_Reg *lib;
    for (lib = loadedlibs; lib->func; lib++) {
        luaL_requiref(L, lib->name, lib->func, 1);
        lua_pop(L, 1);
    }
    ::bee::lua::preload_module(L);
}

#endif
