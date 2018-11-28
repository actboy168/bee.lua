#include <bee/filewatch.h>
#include <bee/exception/windows_exception.h>
#include <bee/lua/binding.h>

namespace luafw {
	static bee::filewatch& to(lua_State* L) {
		return *(bee::filewatch*)lua_touserdata(L, lua_upvalueindex(1));
	}
	static int add(lua_State* L) {
		bee::filewatch& self = to(L);
		auto path = bee::lua::to_string(L, 1);
		int filter = 0;
		const char* sf = luaL_checkstring(L, 2);
		for (const char* f = sf; *f; ++f) {
			switch (*f) {
			case 'f': filter |= bee::filewatch::WatchFile; break;
			case 'd': filter |= bee::filewatch::WatchDir; break;
			case 't': filter |= bee::filewatch::WatchTime; break;
			case 's': filter |= bee::filewatch::WatchSubtree; break;
			case 'l': filter |= bee::filewatch::DisableDelete; break;
			}
		}
		bee::filewatch::taskid id = self.add(path, filter);
		if (id == bee::filewatch::kInvalidTaskId) {
			lua_pushnil(L);
			lua_pushstring(L, bee::w2u(bee::error_message()).c_str());
			return 2;
		}
		lua_pushinteger(L, id);
		return 1;
	}

	static int remove(lua_State* L) {
		bee::filewatch& self = to(L);
		self.remove((bee::filewatch::taskid)luaL_checkinteger(L, 1));
		return 0;
	}

	static int select(lua_State* L) {
		bee::filewatch& self = to(L);
		bee::filewatch::notify notify;
		if (!self.pop(notify)) {
			return 0;
		}
		lua_pushinteger(L, notify.id);
		switch (notify.type) {
		case bee::filewatch::tasktype::Error:
			lua_pushstring(L, "error");
			break;
		case bee::filewatch::tasktype::Create:
			lua_pushstring(L, "create");
			break;
		case bee::filewatch::tasktype::Delete:
			lua_pushstring(L, "delete");
			break;
		case bee::filewatch::tasktype::Modify:
			lua_pushstring(L, "modify");
			break;
		case bee::filewatch::tasktype::RenameFrom:
			lua_pushstring(L, "rename from");
			break;
		case bee::filewatch::tasktype::RenameTo:
			lua_pushstring(L, "rename to");
			break;
		default:
			lua_pushstring(L, "unknown");
			break;
		}
		bee::lua::push_string(L, notify.message);
		return 3;
	}

	static int gc(lua_State* L) {
		bee::filewatch& self = to(L);
		self.~filewatch();
		return 0;
	}
}

extern "C"
#if defined(_WIN32)
__declspec(dllexport)
#endif
int luaopen_bee_filewatch(lua_State* L) {
	bee::filewatch* fw = (bee::filewatch*)lua_newuserdata(L, sizeof(bee::filewatch));
    new (fw)bee::filewatch;

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
