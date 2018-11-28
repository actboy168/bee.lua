#include <bee/fsevent.h>
#include <bee/exception/windows_exception.h>
#include <bee/lua/binding.h>

namespace luafw {
	static bee::fsevent::watch& to(lua_State* L) {
		return *(bee::fsevent::watch*)lua_touserdata(L, lua_upvalueindex(1));
	}
	static int add(lua_State* L) {
		bee::fsevent::watch& self = to(L);
		auto path = bee::lua::to_string(L, 1);
		bee::fsevent::taskid id = self.add(path);
		if (id == bee::fsevent::kInvalidTaskId) {
			lua_pushnil(L);
			lua_pushstring(L, bee::w2u(bee::error_message()).c_str());
			return 2;
		}
		lua_pushinteger(L, id);
		return 1;
	}

	static int remove(lua_State* L) {
		bee::fsevent::watch& self = to(L);
		self.remove((bee::fsevent::taskid)luaL_checkinteger(L, 1));
		return 0;
	}

	static int select(lua_State* L) {
		bee::fsevent::watch& self = to(L);
		bee::fsevent::notify notify;
		if (!self.select(notify)) {
			return 0;
		}
		switch (notify.type) {
		case bee::fsevent::tasktype::Error:
			lua_pushstring(L, "error");
			break;
		case bee::fsevent::tasktype::Create:
			lua_pushstring(L, "create");
			break;
		case bee::fsevent::tasktype::Delete:
			lua_pushstring(L, "delete");
			break;
		case bee::fsevent::tasktype::Modify:
			lua_pushstring(L, "modify");
			break;
		case bee::fsevent::tasktype::Rename:
			lua_pushstring(L, "rename");
			break;
		default:
			lua_pushstring(L, "unknown");
			break;
		}
		bee::lua::push_string(L, notify.path);
		return 2;
	}

	static int gc(lua_State* L) {
		bee::fsevent::watch& self = to(L);
		self.~watch();
		return 0;
	}
}

extern "C"
#if defined(_WIN32)
__declspec(dllexport)
#endif
int luaopen_bee_filewatch(lua_State* L) {
	bee::fsevent::watch* fw = (bee::fsevent::watch*)lua_newuserdata(L, sizeof(bee::fsevent::watch));
    new (fw)bee::fsevent::watch;

    static luaL_Reg lib[] = {
        { "add",    luafw::add },
        { "remove", luafw::remove },
        { "select", luafw::select },
        { "__gc",   luafw::gc },
        { NULL, NULL }
    };
    lua_newtable(L);
    lua_pushvalue(L, -2);
    luaL_setfuncs(L, lib, 1);
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    lua_setmetatable(L, -2);
    return 1;
}
