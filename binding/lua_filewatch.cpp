#include <bee/error.h>
#include <bee/fsevent.h>
#include <bee/lua/binding.h>

namespace bee::lua_filewatch {
    static fsevent::watch& to(lua_State* L) {
        return *(fsevent::watch*)lua_touserdata(L, lua_upvalueindex(1));
    }
    static int add(lua_State* L) {
        fsevent::watch& self = to(L);
        auto            path = lua::checkstring(L, 1);
        fsevent::taskid id = self.add(path);
        if (id == fsevent::kInvalidTaskId) {
            lua_pushnil(L);
            lua_pushstring(L, make_syserror().what());
            return 2;
        }
        lua_pushinteger(L, id);
        return 1;
    }

    static int remove(lua_State* L) {
        fsevent::watch& self = to(L);
        self.remove((fsevent::taskid)luaL_checkinteger(L, 1));
        return 0;
    }

    static int select(lua_State* L) {
        fsevent::watch& self = to(L);
        fsevent::notify notify;
        if (!self.select(notify)) {
            return 0;
        }
        switch (notify.type) {
        case fsevent::tasktype::Error:
            lua_pushstring(L, "error");
            break;
        case fsevent::tasktype::Confirm:
            lua_pushstring(L, "confirm");
            break;
        case fsevent::tasktype::Modify:
            lua_pushstring(L, "modify");
            break;
        case fsevent::tasktype::Rename:
            lua_pushstring(L, "rename");
            break;
        default:
            lua_pushstring(L, "unknown");
            break;
        }
        lua::push_string(L, notify.path);
        return 2;
    }

    static int toclose(lua_State* L) {
        fsevent::watch& self = to(L);
        self.stop();
        return 0;
    }

    static int gc(lua_State* L) {
        fsevent::watch& self = to(L);
        self.~watch();
        return 0;
    }

    static int luaopen(lua_State* L) {
        fsevent::watch* fw = (fsevent::watch*)lua_newuserdatauv(L, sizeof(fsevent::watch), 0);
        new (fw) fsevent::watch;

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
