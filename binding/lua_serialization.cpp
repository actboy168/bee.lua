#include <3rd/lua-seri/lua-seri.h>
#include <bee/lua/binding.h>
#include <bee/lua/module.h>

namespace bee::lua_serialization {
    static int unpack(lua_State* L) {
        switch (lua_type(L, 1)) {
        case LUA_TLIGHTUSERDATA:
            return seri_unpackptr(L, lua::tolightud<void*>(L, 1));
        case LUA_TUSERDATA:
            lua_settop(L, 1);
            return seri_unpack(L, lua::tolightud<void*>(L, 1));
        case LUA_TSTRING:
            lua_settop(L, 1);
            return seri_unpack(L, (void*)lua_tostring(L, 1));
        case LUA_TFUNCTION: {
            lua_settop(L, 1);
            lua_call(L, 0, 3);
            void* data = lua_touserdata(L, -3);
            lua_copy(L, -1, 1);
            lua_toclose(L, 1);
            lua_pop(L, 2);
            return seri_unpack(L, (void*)data);
        }
        default:
            return luaL_error(L, "unsupported type %s", luaL_typename(L, lua_type(L, 1)));
        }
    }

    static int pack(lua_State* L) {
        void* data = seri_pack(L, 0, NULL);
        lua_pushlightuserdata(L, data);
        return 1;
    }
    static int packstring(lua_State* L) {
        int sz;
        void* data = seri_pack(L, 0, &sz);
        lua_pushlstring(L, (const char*)data, sz);
        free(data);
        return 1;
    }
    static int lightuserdata(lua_State* L) {
        luaL_checktype(L, 1, LUA_TUSERDATA);
        lua_pushlightuserdata(L, lua_touserdata(L, 1));
        return 1;
    }
    static int luaopen(lua_State* L) {
        luaL_Reg lib[] = {
            { "unpack", unpack },
            { "pack", pack },
            { "packstring", packstring },
            { "lightuserdata", lightuserdata },
            { NULL, NULL }
        };
        luaL_newlibtable(L, lib);
        luaL_setfuncs(L, lib, 0);
        return 1;
    }
}

DEFINE_LUAOPEN(serialization)
