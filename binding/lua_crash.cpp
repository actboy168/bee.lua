#include <bee/lua/module.h>
#include <bee/lua/udata.h>
#include <bee/crash/handler.h>

namespace bee::lua_crash {
    static int create_handler(lua_State* L) {
        const char* dump_path = luaL_checkstring(L, 1);
        lua::newudata<bee::crash::handler>(L, dump_path);
        return 1;
    }

    static void metatable(lua_State* L) {
    }

    static int luaopen(lua_State* L) {
        static luaL_Reg lib[] = {
            { "create_handler", create_handler },
            { NULL, NULL },
        };
        luaL_newlibtable(L, lib);
        luaL_setfuncs(L, lib, 0);
        return 1;
    }
}

DEFINE_LUAOPEN(crash)

namespace bee::lua {
    template <>
    struct udata<bee::crash::handler> {
        static inline auto metatable = bee::lua_crash::metatable;
    };
}
