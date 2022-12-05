#include <bee/error.h>
#include <bee/filewatch.h>
#include <bee/lua/binding.h>
#include <bee/utility/unreachable.h>

namespace bee::lua_filewatch {
    static filewatch::watch& to(lua_State* L, int idx) {
        return *(filewatch::watch*)getObject(L, idx, "filewatch");
    }
    static int add(lua_State* L) {
        filewatch::watch& self = to(L, 1);
        auto path = lua::to_string(L, 2);
        std::error_code ec;
        fs::path abspath = fs::absolute(path, ec);
        if (ec) {
            lua::push_errormesg(L, "fs::absolute", ec);
            lua_error(L);
            return 0;
        }
        self.add(abspath.lexically_normal());
        return 0;
    }

    static int select(lua_State* L) {
        filewatch::watch& self = to(L, 1);
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
        filewatch::watch& self = to(L, 1);
        self.stop();
        return 0;
    }

    static int gc(lua_State* L) {
        filewatch::watch& self = to(L, 1);
        self.~watch();
        return 0;
    }

    static int create(lua_State* L) {
        void* storage = lua_newuserdatauv(L, sizeof(filewatch::watch), 0);
        if (newObject(L, "filewatch")) {
            static luaL_Reg mt[] = {
                {"add", add},
                {"select", select},
                {"__close", toclose},
                {"__gc", gc},
                {NULL, NULL}
            };
            luaL_setfuncs(L, mt, 0);
            lua_pushvalue(L, -1);
            lua_setfield(L, -2, "__index");
        }
        lua_setmetatable(L, -2);
        new (storage) filewatch::watch;
        return 1;
    }

    static int luaopen(lua_State* L) {
        static luaL_Reg lib[] = {
            {"create", create},
            {NULL, NULL}
        };
        lua_newtable(L);
        luaL_setfuncs(L, lib, 0);
        return 1;
    }
}

DEFINE_LUAOPEN(filewatch)
