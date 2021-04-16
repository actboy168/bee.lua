#include <bee/lua/binding.h>
#include <bee/platform.h>
#include <lua.hpp>

namespace bee::lua_platform {
    static int luaopen(lua_State* L) {
        lua_newtable(L);
        lua_pushstring(L, BEE_OS_NAME);
        lua_setfield(L, -2, "OS");
        lua_pushstring(L, BEE_ARCH);
        lua_setfield(L, -2, "Arch");
        lua_pushstring(L, BEE_CRT_NAME);
        lua_setfield(L, -2, "CRT");
        lua_pushstring(L, BEE_COMPILER_NAME);
        lua_setfield(L, -2, "Compiler");
        lua_pushboolean(L, BEE_DEBUG);
        lua_setfield(L, -2, "DEBUG");
        lua_pushstring(L, BEE_COMPILER_VERSION);
        lua_setfield(L, -2, "CompilerVersion");
        lua_pushstring(L, BEE_CRT_VERSION);
        lua_setfield(L, -2, "CRTVersion");
        return 1;
    }
}

DEFINE_LUAOPEN(platform)
