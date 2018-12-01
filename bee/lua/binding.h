#pragma once

#include <bee/utility/unicode.h>
#include <lua.hpp>

namespace bee::lua {
#if defined(_WIN32)
	typedef std::wstring string_type;
#else
	typedef std::string string_type;
#endif

	inline int push_error(lua_State* L, const std::exception& e)
	{
#if defined(_WIN32)
		lua_pushstring(L, bee::a2u(e.what()).c_str());
#else
		lua_pushstring(L, e.what());
#endif
		return lua_error(L);
	}

	inline std::string_view to_strview(lua_State* L, int idx)
	{
		size_t len = 0;
		const char* buf = luaL_checklstring(L, idx, &len);
		return std::string_view(buf, len);
	}

	inline string_type to_string(lua_State* L, int idx)
	{
		size_t len = 0;
		const char* buf = luaL_checklstring(L, idx, &len);
#if defined(_WIN32)
		return bee::u2w(std::string_view(buf, len));
#else
		return std::string(buf, len);
#endif
	}

	inline void push_string(lua_State* L, const string_type& str)
	{
#if defined(_WIN32) 
		std::string utf8 = bee::w2u(str);
		lua_pushlstring(L, utf8.data(), utf8.size());
#else
		lua_pushlstring(L, str.data(), str.size());
#endif
	}
}

#define LUA_TRY     try {   
#define LUA_TRY_END } catch (const std::exception& e) { return ::bee::lua::push_error(L, e); }
