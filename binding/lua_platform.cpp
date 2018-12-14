#include <lua.hpp>      
#include <bee/lua/binding.h>
#include <bee/platform.h>

namespace bee::lua_platform {
    int os(lua_State* L) {
        lua_pushstring(L, BEE_OS_NAME);
        return 1;
    }
    int compiler(lua_State* L) {
        lua_pushstring(L, BEE_COMPILER_NAME);
        return 1;
    }
    int crt(lua_State* L) {
        lua_pushstring(L, BEE_CRT_NAME);
        return 1;
    }
    int arch(lua_State* L) {
        lua_pushstring(L, BEE_STRINGIZE(BEE_ARCH));
        return 1;
    }
    int debug(lua_State* L) {
        lua_pushboolean(L, BEE_DEBUG);
        return 1;
    }
    int luaopen(lua_State* L) {
        luaL_Reg lib[] = {
            { "os", os },
            { "compiler", compiler },
            { "crt", crt },
            { "arch", arch },
            { "debug", debug },
            { NULL, NULL }
        };
        luaL_newlib(L, lib);
        return 1;
    }
}

DEFINE_LUAOPEN(platform)
