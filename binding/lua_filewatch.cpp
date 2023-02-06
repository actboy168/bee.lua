#include <bee/error.h>
#include <bee/filewatch.h>
#include <bee/lua/binding.h>
#include <bee/utility/unreachable.h>
#include <bee/filesystem.h>

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
        self.add(abspath.lexically_normal().string<filewatch::watch::string_type::value_type>());
        return 0;
    }

    static int set_recursive(lua_State* L) {
        filewatch::watch& self = to(L, 1);
        bool enable = lua_toboolean(L, 2);
        self.set_recursive(enable);
        lua_pushboolean(L, 1);
        return 1;
    }

    static int set_follow_symlinks(lua_State* L) {
        filewatch::watch& self = to(L, 1);
        bool enable = lua_toboolean(L, 2);
        bool ok = self.set_follow_symlinks(enable);
        lua_pushboolean(L, ok);
        return 1;
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
        lua_pushlstring(L, notify->path.data(), notify->path.size());
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
                {"set_recursive", set_recursive},
                {"set_follow_symlinks", set_follow_symlinks},
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
            {"type", NULL},
            {NULL, NULL}
        };
        lua_newtable(L);
        luaL_setfuncs(L, lib, 0);
        lua_pushstring(L, filewatch::watch::type());
        lua_setfield(L, -2, "type");
        return 1;
    }
}

DEFINE_LUAOPEN(filewatch)
