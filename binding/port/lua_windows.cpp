#include <bee/lua/binding.h>
#include <bee/platform/win/unicode.h>
#include <binding/binding.h>
#include <binding/file.h>
#include <fcntl.h>
#include <io.h>

namespace bee::lua_windows {
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

    static int filemode(lua_State* L) {
        luaL_Stream* p = lua::tofile(L, 1);
        auto mode      = lua::checkstrview(L, 2);
        if (p && p->closef && p->f) {
            int ok = _setmode(_fileno(p->f), mode[0] == 'b' ? _O_BINARY : _O_TEXT);
            if (ok == -1) {
                lua_pushnil(L);
                lua_pushstring(L, make_crterror("_setmode").c_str());
                return 2;
            }
            lua_pushboolean(L, 1);
            return 1;
        }
        lua_pushnil(L);
        lua_pushstring(L, make_error(std::make_error_code(std::errc::bad_file_descriptor), "_setmode").c_str());
        return 2;
    }

    static int luaopen(lua_State* L) {
        luaL_Reg lib[] = {
            { "u2a", lu2a },
            { "a2u", la2u },
            { "filemode", filemode },
            { NULL, NULL }
        };
        luaL_newlibtable(L, lib);
        luaL_setfuncs(L, lib, 0);
        return 1;
    }
}

DEFINE_LUAOPEN(windows)
