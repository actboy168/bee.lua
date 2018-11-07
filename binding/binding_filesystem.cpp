#include <lua.hpp>	  		
#include <filesystem>
#include <bee/utility/path_helper.h>
#include <bee/utility/unicode.h>
#include <bee/lua/range.h>
#include <bee/lua/binding.h>

namespace fs = std::filesystem;

namespace luafs {
	namespace path {
		class directory_container
		{
		public:
			directory_container(directory_container const& that) : p_(that.p_) { }
			directory_container(fs::path const& that) : p_(that) { }
			fs::directory_iterator cbegin() const {
				std::error_code ec;
				auto r = fs::directory_iterator(p_, ec);
				if (!!ec)
				{
					return fs::directory_iterator();
				}
				return r;
			}
			fs::directory_iterator begin() const { return cbegin(); }
			fs::directory_iterator begin()       { return cbegin(); }
			fs::directory_iterator end() const   { return fs::directory_iterator(); }
			fs::directory_iterator end()         { return fs::directory_iterator(); }
		private:
			const fs::path& p_;
		};

		static void* newudata(lua_State* L)
		{
			void* storage = lua_newuserdata(L, sizeof(fs::path));
			luaL_getmetatable(L, "filesystem");
			lua_setmetatable(L, -2);
			return storage;
		}

		static fs::path& to(lua_State* L, int idx)
		{
			return *(fs::path*)luaL_checkudata(L, idx, "filesystem");
		}

		static int constructor_(lua_State* L)
		{
			void* storage = newudata(L);
			new (storage)fs::path();
			return 1;
		}

		static int constructor_(lua_State* L, const fs::path::string_type& path)
		{
			void* storage = newudata(L);
			new (storage)fs::path(path);
			return 1;
		}

		static int constructor_(lua_State* L, fs::path::string_type&& path)
		{
			void* storage = newudata(L);
			new (storage)fs::path(std::forward<fs::path::string_type>(path));
			return 1;
		}

		static int constructor_(lua_State* L, const fs::path& path)
		{
			void* storage = newudata(L);
			new (storage)fs::path(path);
			return 1;
		}

		static int constructor_(lua_State* L, fs::path&& path)
		{
			void* storage = newudata(L);
			new (storage)fs::path(std::forward<fs::path>(path));
			return 1;
		}

		static int constructor(lua_State* L)
		{
			LUA_TRY;
			if (lua_gettop(L) == 0) {
				return constructor_(L);
			}
			switch (lua_type(L, 1)) {
			case LUA_TSTRING:
				return constructor_(L, ::bee::lua::to_string(L, 1));
			case LUA_TUSERDATA:
				return constructor_(L, to(L, 1));
			}
			luaL_checktype(L, 1, LUA_TSTRING);
			return 0;
			LUA_TRY_END;
		}

		static int string(lua_State* L)
		{
			LUA_TRY;
			const fs::path& self = path::to(L, 1);
			::bee::lua::push_string(L, self.string<fs::path::value_type>());
			return 1;
			LUA_TRY_END;
		}

		static int filename(lua_State* L)
		{
			LUA_TRY;
			const fs::path& self = path::to(L, 1);
			return constructor_(L, std::move(self.filename()));
			LUA_TRY_END;
		}

		static int parent_path(lua_State* L)
		{
			LUA_TRY;
			const fs::path& self = path::to(L, 1);
			return constructor_(L, std::move(self.parent_path()));
			LUA_TRY_END;
		}

		static int stem(lua_State* L)
		{
			LUA_TRY;
			const fs::path& self = path::to(L, 1);
			return constructor_(L, std::move(self.stem()));
			LUA_TRY_END;
		}

		static int extension(lua_State* L)
		{
			LUA_TRY;
			const fs::path& self = path::to(L, 1);
			return constructor_(L, std::move(self.extension()));
			LUA_TRY_END;
		}

		static int is_absolute(lua_State* L)
		{
			LUA_TRY;
			const fs::path& self = path::to(L, 1);
			lua_pushboolean(L, self.is_absolute());
			return 1;
			LUA_TRY_END;
		}

		static int is_relative(lua_State* L)
		{
			LUA_TRY;
			const fs::path& self = path::to(L, 1);
			lua_pushboolean(L, self.is_relative());
			return 1;
			LUA_TRY_END;
		}

		static int remove_filename(lua_State* L)
		{
			LUA_TRY;
			fs::path& self = path::to(L, 1);
			self.remove_filename();
			return 1;
			LUA_TRY_END;
		}

		static int replace_extension(lua_State* L)
		{
			LUA_TRY;
			fs::path& self = path::to(L, 1);
			switch (lua_type(L, 2)) {
			case LUA_TSTRING:
				self.replace_extension(::bee::lua::to_string(L, 2));
				lua_settop(L, 1);
				return 1;
			case LUA_TUSERDATA:
				self.replace_extension(to(L, 2));
				lua_settop(L, 1);
				return 1;
			}
			luaL_checktype(L, 2, LUA_TSTRING);
			return 0;
			LUA_TRY_END;
		}

		static int list_directory(lua_State* L)
		{
			LUA_TRY;
			const fs::path& self = path::to(L, 1);  
			bee::lua::make_range(L, directory_container(self));
			lua_pushnil(L);
			lua_pushnil(L);
			return 3;
			LUA_TRY_END;
		}

		static int permissions(lua_State* L)
		{
			LUA_TRY;
			const fs::path& self = path::to(L, 1);
			lua_pushinteger(L, lua_Integer(fs::status(self).permissions()));
			return 1;
			LUA_TRY_END;
		}

		static int add_permissions(lua_State* L)
		{
			LUA_TRY;
			const fs::path& self = path::to(L, 1);
			fs::perms perms = fs::perms::mask & fs::perms(luaL_checkinteger(L, 2));
			fs::permissions(self, perms, fs::perm_options::add);
			return 0;
			LUA_TRY_END;
		}

		static int remove_permissions(lua_State* L)
		{
			LUA_TRY;
			const fs::path& self = path::to(L, 1);
			fs::perms perms = fs::perms::mask & fs::perms(luaL_checkinteger(L, 2));
			fs::permissions(self, perms, fs::perm_options::remove);
			return 0;
			LUA_TRY_END;
		}

		static int mt_div(lua_State* L)
		{
			LUA_TRY;
			const fs::path& self = path::to(L, 1);
			switch (lua_type(L, 2)) {
			case LUA_TSTRING:
				return constructor_(L, std::move(self / ::bee::lua::to_string(L, 2)));
			case LUA_TUSERDATA:
				return constructor_(L, std::move(self / to(L, 2)));
			}
			luaL_checktype(L, 2, LUA_TSTRING);
			return 0;
			LUA_TRY_END;
		}

		static int mt_eq(lua_State* L)
		{
			LUA_TRY;
			const fs::path& self = path::to(L, 1);
			const fs::path& rht = path::to(L, 2);
			lua_pushboolean(L, bee::path::equal(self, rht));
			return 1;
			LUA_TRY_END;
		}

		static int destructor(lua_State* L)
		{
			LUA_TRY;
			fs::path& self = path::to(L, 1);
			self.~path();
			return 0;
			LUA_TRY_END;
		}

		static int mt_tostring(lua_State* L)
		{
			LUA_TRY;
			const fs::path& self = path::to(L, 1);
			::bee::lua::push_string(L, self.string<fs::path::value_type>());
			return 1;
			LUA_TRY_END;
		}
	}

	static int exists(lua_State* L)
	{
		LUA_TRY;
		const fs::path& p = path::to(L, 1);
		lua_pushboolean(L, fs::exists(p));
		return 1; 
		LUA_TRY_END;
	}

	static int is_directory(lua_State* L)
	{
		LUA_TRY;
		const fs::path& p = path::to(L, 1);
		lua_pushboolean(L, fs::is_directory(p));
		return 1;
		LUA_TRY_END;
	}

	static int create_directory(lua_State* L)
	{
		LUA_TRY;
		const fs::path& p = path::to(L, 1);
		lua_pushboolean(L, fs::create_directory(p));
		return 1;
		LUA_TRY_END;
	}

	static int create_directories(lua_State* L)
	{
		LUA_TRY;
		const fs::path& p = path::to(L, 1);
		lua_pushboolean(L, fs::create_directories(p));
		return 1;
		LUA_TRY_END;
	}

	static int rename(lua_State* L)
	{
		LUA_TRY;
		const fs::path& from = path::to(L, 1);
		const fs::path& to = path::to(L, 2);
		fs::rename(from, to);
		return 0;
		LUA_TRY_END;
	}

	static int remove(lua_State* L)
	{
		LUA_TRY;
		const fs::path& p = path::to(L, 1);
		lua_pushboolean(L, fs::remove(p));
		return 1;
		LUA_TRY_END;
	}

	static int remove_all(lua_State* L)
	{
		LUA_TRY;
		const fs::path& p = path::to(L, 1);
		lua_pushinteger(L, fs::remove_all(p));
		return 1;
		LUA_TRY_END;
	}

	static int current_path(lua_State* L)
	{
		LUA_TRY;
		if (lua_gettop(L) == 0) {
			return path::constructor_(L, std::move(fs::current_path()));
		}
		const fs::path& p = path::to(L, 1);
		fs::current_path(p);
		return 0;
		LUA_TRY_END;
	}

	static int copy_file(lua_State* L)
	{
		LUA_TRY;
		const fs::path& from = path::to(L, 1);
		const fs::path& to = path::to(L, 2);
		bool overwritten = !!lua_toboolean(L, 3);
		fs::copy_file(from, to, overwritten ? fs::copy_options::overwrite_existing : fs::copy_options::none);
		return 0;
		LUA_TRY_END;
	}

	static int absolute(lua_State* L)
	{
		LUA_TRY;
		const fs::path& p = path::to(L, 1);
		if (lua_gettop(L) == 1) {
			return path::constructor_(L, std::move(fs::absolute(p)));
		}
		const fs::path& base = path::to(L, 2);
		return path::constructor_(L, std::move(fs::absolute(base / p)));
		LUA_TRY_END;
	}

	static int relative(lua_State* L)
	{
		LUA_TRY;
		const fs::path& p = path::to(L, 1);
		const fs::path& base = path::to(L, 2);
		return path::constructor_(L, std::move(fs::relative(p, base)));
		LUA_TRY_END;
	}

	static int last_write_time(lua_State* L)
	{
		LUA_TRY;
		const fs::path& p = path::to(L, 1);
		if (lua_gettop(L) == 1) {
			fs::file_time_type time = fs::last_write_time(p);
			lua_pushinteger(L, time.time_since_epoch().count());
			return 1;
		}
		fs::last_write_time(p, fs::file_time_type() + fs::file_time_type::duration(luaL_checkinteger(L, 2)));
		return 0;
		LUA_TRY_END;
	}

	static int procedure_path(lua_State* L)
	{
		LUA_TRY;
		return path::constructor_(L, std::move(bee::path::module().parent_path()));
		LUA_TRY_END;
	}
}
 
namespace bee { namespace lua {
	template <>
	int convert_to_lua(lua_State* L, const fs::directory_entry& v)
	{
		luafs::path::constructor_(L, v.path());
		return 1;
	}
}}

extern "C" __declspec(dllexport)
int luaopen_bee_filesystem(lua_State* L)
{
	static luaL_Reg mt[] = {
		{ "string", luafs::path::string },
		{ "filename", luafs::path::filename },
		{ "parent_path", luafs::path::parent_path },
		{ "stem", luafs::path::stem },
		{ "extension", luafs::path::extension },
		{ "is_absolute", luafs::path::is_absolute },
		{ "is_relative", luafs::path::is_relative },
		{ "remove_filename", luafs::path::remove_filename },
		{ "replace_extension", luafs::path::replace_extension },
		{ "list_directory", luafs::path::list_directory },
		{ "permissions", luafs::path::permissions },
		{ "add_permissions", luafs::path::add_permissions },
		{ "remove_permissions", luafs::path::remove_permissions },
		{ "__div", luafs::path::mt_div },
		{ "__eq", luafs::path::mt_eq },
		{ "__gc", luafs::path::destructor },
		{ "__tostring", luafs::path::mt_tostring },
		{ "__debugger_tostring", luafs::path::mt_tostring },
		{ NULL, NULL }
	};
	luaL_newmetatable(L, "filesystem");
	luaL_setfuncs(L, mt, 0);
	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index");

	static luaL_Reg lib[] = {
		{ "path", luafs::path::constructor },
		{ "exists", luafs::exists },
		{ "is_directory", luafs::is_directory },
		{ "create_directory", luafs::create_directory },
		{ "create_directories", luafs::create_directories },
		{ "rename", luafs::rename },
		{ "remove", luafs::remove },
		{ "remove_all", luafs::remove_all },
		{ "current_path", luafs::current_path },
		{ "copy_file", luafs::copy_file },
		{ "absolute", luafs::absolute },
		{ "relative", luafs::relative },
		{ "last_write_time", luafs::last_write_time },
		{ "procedure_path", luafs::procedure_path },
		{ NULL, NULL }
	};	
	lua_newtable(L);
	luaL_setfuncs(L, lib, 0);
	return 1;
}
