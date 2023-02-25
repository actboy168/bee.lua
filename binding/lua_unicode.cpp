#include <binding/binding.h>
#include <bee/platform/win/unicode.h>

namespace bee::lua_unicode {
    static int lu2a(lua_State* L) {
        auto r = win::u2a(lua::checkstrview(L, 1));
        lua_pushlstring(L, r.data(), r.size());
        return 1;
    }

    static int la2u(lua_State* L) {
        auto r = win::a2u(lua::checkstrview(L, 1));
        lua_pushlstring(L, r.data(), r.size());
        return 1;
    }

    static int luaopen(lua_State* L) {
        luaL_Reg lib[] = {
            {"u2a", lu2a},
            {"a2u", la2u},
            {NULL, NULL}
        };
        luaL_newlibtable(L, lib);
        luaL_setfuncs(L, lib, 0);
        return 1;
    }
}

DEFINE_LUAOPEN(unicode)
