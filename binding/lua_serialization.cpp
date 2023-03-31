#include <binding/binding.h>

extern "C" {
#include <3rd/lua-seri/lua-seri.h>
}

namespace bee::lua_serialization {
    static int unpack(lua_State* L) {
        switch (lua_type(L, 1)) {
        case LUA_TLIGHTUSERDATA:
            return seri_unpackptr(L, lua::tolightud<void*>(L, 1));
        case LUA_TSTRING: {
            return seri_unpack(L);
        default:
            return luaL_error(L, "unsupported type %s", luaL_typename(L, lua_type(L, 1)));
        }
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
        switch (lua_type(L, 1)) {
        case LUA_TLIGHTUSERDATA:
            lua_pushinteger(L, lua::tolightud<lua_Integer>(L, 1));
            return 1;
        case LUA_TNUMBER:
            if (lua_isinteger(L, 1)) {
                lua_pushlightuserdata(L, (void*)lua::checkinteger<intptr_t>(L, 1));
                return 1;
            }
            else {
                return luaL_error(L, "unsupported type float");
            }
        default:
            return luaL_error(L, "unsupported type %s", luaL_typename(L, lua_type(L, 1)));
        }
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
