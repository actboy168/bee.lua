#include <bee/error.h>
#include <bee/filewatch.h>
#include <binding/binding.h>
#include <bee/nonstd/unreachable.h>
#include <bee/nonstd/filesystem.h>

namespace bee::lua {
    template <>
    struct udata<filewatch::watch> {
        static inline int  nupvalue = 1;
        static inline auto name = "bee::filewatch";
    };
}

namespace bee::lua_filewatch {
    static filewatch::watch& to(lua_State* L, int idx) {
        return lua::checkudata<filewatch::watch>(L, idx);
    }

    static lua_State* get_thread(lua_State* L) {
        lua_getiuservalue(L, 1, 1);
        lua_State* thread = lua_tothread(L, -1);
        lua_pop(L, 1);
        return thread;
    }

    static int add(lua_State* L) {
        filewatch::watch& self = to(L, 1);
        auto path = lua::checkstring(L, 2);
        std::error_code ec;
        fs::path abspath = fs::absolute(path, ec);
        if (ec) {
            auto error = std::system_error(ec, "fs::absolute");
            lua_pushstring(L, error.what());
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

    static int set_filter(lua_State* L) {
        filewatch::watch& self = to(L, 1);
        if (lua_isnoneornil(L, 2)) {
            bool ok = self.set_filter();
            lua_pushboolean(L, ok);
            return 1;
        }
        lua_State* thread = get_thread(L);
        lua_settop(L, 2);
        lua_xmove(L, thread, 1);
        if (lua_gettop(thread) > 1) {
            lua_replace(thread, 1);
        }
        bool ok = self.set_filter([=](const char* path){
            lua_pushvalue(thread, 1);
            lua_pushstring(thread, path);
            if (LUA_OK != lua_pcall(thread, 1, 1, 0)) {
                lua_pop(thread, 1);
                return true;
            }
            bool r = lua_toboolean(thread, -1);
            lua_pop(thread, 1);
            return r;
        });
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
            std::unreachable();
        }
        lua_pushlstring(L, notify->path.data(), notify->path.size());
        return 2;
    }

    static int mt_close(lua_State* L) {
        filewatch::watch& self = to(L, 1);
        self.stop();
        return 0;
    }

    static void metatable(lua_State* L) {
        static luaL_Reg lib[] = {
            {"add", add},
            {"set_recursive", set_recursive},
            {"set_follow_symlinks", set_follow_symlinks},
            {"set_filter", set_filter},
            {"select", select},
            {NULL, NULL}
        };
        luaL_newlibtable(L, lib);
        luaL_setfuncs(L, lib, 0);
        lua_setfield(L, -2, "__index");
        static luaL_Reg mt[] = {
            {"__close", mt_close},
            {NULL, NULL}
        };
        luaL_setfuncs(L, mt, 0);
    }

    static int create(lua_State* L) {
        lua::newudata<filewatch::watch>(L, metatable);
        lua_newthread(L);
        lua_setiuservalue(L, -2, 1);
        return 1;
    }

    static int luaopen(lua_State* L) {
        static luaL_Reg lib[] = {
            {"create", create},
            {"type", NULL},
            {NULL, NULL}
        };
        luaL_newlibtable(L, lib);
        luaL_setfuncs(L, lib, 0);
        lua_pushstring(L, filewatch::watch::type());
        lua_setfield(L, -2, "type");
        return 1;
    }
}

DEFINE_LUAOPEN(filewatch)
