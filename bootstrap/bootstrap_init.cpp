#include <binding/binding.h>

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

extern "C" void luaL_openlibs(lua_State *L) {
    const luaL_Reg *lib;
    for (lib = loadedlibs; lib->func; lib++) {
        luaL_requiref(L, lib->name, lib->func, 1);
        lua_pop(L, 1);
    }
    ::bee::lua::preload_module(L);
}
