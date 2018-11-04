#include <lua.hpp>	  
#include <bee/registry/key.h>
#include <bee/utility/unicode.h>
#include <bee/lua/binding.h>

namespace luareg {

	using namespace bee::registry;

	key_w* rkey_read(lua_State* L, int idx)
	{
		return static_cast<key_w*>(lua_touserdata(L, idx));
	}

	int rkey_push_blob(lua_State* L, const std::dynarray<uint8_t>& value)
	{
		lua_pushlstring(L, (const char*)value.data(), value.size());
		return 1;
	}

	key_w* rkey_create(lua_State* L, key_w::hkey_type keytype, open_access::t access)
	{
		key_w* storage = (key_w*)lua_newuserdata(L, sizeof(key_w));
		lua_pushvalue(L, lua_upvalueindex(1));
		lua_setmetatable(L, -2);
		new (storage)key_w(keytype, access);
		return storage;
	}

	int rkey_copy(lua_State* L, int ud_idx, const key_w* key)
	{
		void* storage = lua_newuserdata(L, sizeof(key_w));
		lua_getmetatable(L, ud_idx);
		lua_setmetatable(L, -2);
		new (storage)key_w(*key);
		return 1;
	}

	int rkey_index(lua_State* L)
	{
		try {
			key_w*       self = rkey_read(L, 1);
			std::wstring key = ::bee::lua::to_string(L, 2);
			key_w::value_type& value = self->value(key);
			switch (value.type())
			{
			case REG_DWORD:
				lua_pushinteger(L, value.get_uint32_t());
				return 1;
			case REG_QWORD:
				lua_pushinteger(L, value.get_uint64_t());
				return 1;
			case REG_SZ:
			case REG_EXPAND_SZ:
				::bee::lua::push_string(L, value.get_string());
				return 1;
			case REG_BINARY:
				return rkey_push_blob(L, value.get_binary());
			default:
				break;
			}
		} catch (const std::exception&) {
		}
		lua_pushnil(L);
		return 1;
	}

	int rkey_newindex(lua_State* L)
	{
		LUA_TRY;
		{
			key_w*       self = rkey_read(L, 1);
			std::wstring key = ::bee::lua::to_string(L, 2);
			key_w::value_type& value = self->value(key);
			switch (lua_type(L, 3))
			{
			case LUA_TSTRING:
				value.set(::bee::lua::to_string(L, 3));
				return 0;
			case LUA_TNUMBER:
				value.set_uint32_t((uint32_t)lua_tointeger(L, 3));
				return 0;
			case LUA_TTABLE:
			{
				lua_geti(L, 3, 1);
				switch (lua_tointeger(L, -1))
				{
				case REG_DWORD:
					lua_geti(L, 3, 2);
					value.set_uint32_t((uint32_t)lua_tointeger(L, -1));
					break;
				case REG_QWORD:
					lua_geti(L, 3, 2);
					value.set_uint64_t(lua_tointeger(L, -1));
					break;
				case REG_EXPAND_SZ:
				case REG_SZ:
					lua_geti(L, 3, 2);
					value.set(::bee::lua::to_string(L, -1));
					break;
				case REG_MULTI_SZ:
				{
					int len = (int)luaL_len(L, 3);
					std::wstring str = L"";
					for (int i = 2; i <= len; ++i)
					{
						lua_geti(L, 3, i);
						str += ::bee::lua::to_string(L, -1);
						str.push_back(L'\0');
						lua_pop(L, 1);
					}
					str.push_back(L'\0');
					value.set(REG_MULTI_SZ, str.c_str(), str.size() * sizeof(wchar_t));
					break;
				}
				case REG_BINARY:
				{
					lua_geti(L, 3, 2);
					size_t buflen = 0;
					const char* buf = lua_tolstring(L, -1, &buflen);
					value.set((const void*)buf, buflen);
					break;
				}
				default:
					break;
				}
				return 0;
			default:
				return 0;
			}
			}
		}
		LUA_TRY_END;
	}

	int rkey_div(lua_State* L)
	{
		LUA_TRY;
		{
			key_w*       self = rkey_read(L, 1);
			std::wstring rht = ::bee::lua::to_string(L, 2);
			key_w        ret = *self / rht;
			return rkey_copy(L, 1, &ret);
		}
		LUA_TRY_END;
	}

	int rkey_gc(lua_State* L)
	{
		static_cast<key_w*>(lua_touserdata(L, 1))->~key_w();
		return 0;
	}

	int open(lua_State* L)
	{
		std::wstring key = ::bee::lua::to_string(L, 1);
		size_t pos = key.find(L'\\');
		if (pos == -1) {
			return 0;
		}
		std::wstring base =  key.substr(0, pos);
		key_w::hkey_type basetype;
		if (base == L"HKEY_LOCAL_MACHINE") {
			basetype = HKEY_LOCAL_MACHINE;
		}
		else if (base == L"HKEY_CURRENT_USER") {
			basetype = HKEY_CURRENT_USER;
		}
		else {
			return 0;
		}
		std::wstring sub = key.substr(pos + 1);
		key_w* rkey = rkey_create(L, basetype, open_access::none);
		key_w ret = *rkey / sub;
		return rkey_copy(L, lua_absindex(L, -1), &ret);
	}

	int del(lua_State* L)
	{
		key_w* self = rkey_read(L, 1);
		lua_pushboolean(L, self->del()? 1: 0);
		return 1;
	}
}

extern "C" __declspec(dllexport)
int luaopen_bee_registry(lua_State* L)
{
	static luaL_Reg func[] = {
		{ "open", luareg::open },
		{ "del", luareg::del },
		{ NULL, NULL }
	};
	static luaL_Reg rkey_mt[] = {
		{ "__index", luareg::rkey_index },
		{ "__newindex", luareg::rkey_newindex },
		{ "__div", luareg::rkey_div },
		{ "__gc", luareg::rkey_gc },
		{ NULL, NULL }
	};
	luaL_newlibtable(L, func);
	luaL_newlib(L, rkey_mt);
	luaL_setfuncs(L, func, 1);

#define LUA_PUSH_CONST(L, val) \
	lua_pushinteger(L, (val)); \
	lua_setfield(L, -2, # val);

	LUA_PUSH_CONST(L, REG_DWORD);
	LUA_PUSH_CONST(L, REG_QWORD);
	LUA_PUSH_CONST(L, REG_SZ);
	LUA_PUSH_CONST(L, REG_EXPAND_SZ);
	LUA_PUSH_CONST(L, REG_MULTI_SZ);
	LUA_PUSH_CONST(L, REG_BINARY);
	LUA_PUSH_CONST(L, KEY_WOW64_32KEY);
	LUA_PUSH_CONST(L, KEY_WOW64_64KEY);

#undef LUA_PUSH_CONST
	return 1;
}
