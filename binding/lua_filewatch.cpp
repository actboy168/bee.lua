#include <bee/error.h>
#include <bee/filewatch.h>
#include <bee/lua/binding.h>
#include <bee/utility/unreachable.h>

namespace bee::lua_filewatch {
    static filewatch::watch& to(lua_State* L) {
        return *(filewatch::watch*)lua_touserdata(L, lua_upvalueindex(1));
    }
    static int add(lua_State* L) {
        filewatch::watch& self = to(L);
        auto path = lua::to_string(L, 1);
        std::error_code ec;
        fs::path abspath = fs::absolute(path, ec);
        if (ec) {
            lua::push_errormesg(L, "fs::absolute", ec);
            lua_error(L);
            return 0;
        }
        filewatch::taskid id = self.add(abspath.lexically_normal());
        lua_pushinteger(L, id);
        return 1;
    }

    static int remove(lua_State* L) {
        filewatch::watch& self = to(L);
        self.remove((filewatch::taskid)luaL_checkinteger(L, 1));
        return 0;
    }

    static int select(lua_State* L) {
        filewatch::watch& self = to(L);
        self.update();
        auto notify = self.select();
        if (!notify) {
            return 0;
        }
        switch (notify->flags) {
        case filewatch::notify::flag::modify:
            lua_pushstring(L, "modify");
            break;
        case filewatch::notify::flag::rename:
            lua_pushstring(L, "rename");
            break;
        default:
            unreachable();
        }
        lua::push_string(L, notify->path);
        return 2;
    }

    static int toclose(lua_State* L) {
        filewatch::watch& self = to(L);
        self.stop();
        return 0;
    }

    static int gc(lua_State* L) {
        filewatch::watch& self = to(L);
        self.~watch();
        return 0;
    }

    static int luaopen(lua_State* L) {
        filewatch::watch* fw = (filewatch::watch*)lua_newuserdatauv(L, sizeof(filewatch::watch), 0);
        new (fw) filewatch::watch;

        static luaL_Reg lib[] = {
            {"add", add},
            {"remove", remove},
            {"select", select},
            {"__close", toclose},
            {"__gc", gc},
            {NULL, NULL}};
        lua_newtable(L);
        lua_pushvalue(L, -2);
        luaL_setfuncs(L, lib, 1);
        lua_pushvalue(L, -1);
        lua_setfield(L, -2, "__index");
        lua_setmetatable(L, -2);
        return 1;
    }
}

DEFINE_LUAOPEN(filewatch)
