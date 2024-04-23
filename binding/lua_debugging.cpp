#include <bee/lua/module.h>
#include <bee/nonstd/debugging.h>

namespace bee::lua_debugging {
    static int breakpoint(lua_State* L) {
        std::breakpoint();
        return 0;
    }
    static int is_debugger_present(lua_State* L) {
        lua_pushboolean(L, std::is_debugger_present());
        return 1;
    }
    static int breakpoint_if_debugging(lua_State* L) {
        std::breakpoint_if_debugging();
        return 0;
    }

    static int luaopen(lua_State* L) {
        luaL_Reg lib[] = {
            { "breakpoint", breakpoint },
            { "is_debugger_present", is_debugger_present },
            { "breakpoint_if_debugging", breakpoint_if_debugging },
            { NULL, NULL },
        };
        luaL_newlibtable(L, lib);
        luaL_setfuncs(L, lib, 0);
        return 1;
    }
}

DEFINE_LUAOPEN(debugging)
