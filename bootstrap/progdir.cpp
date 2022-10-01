#include <lua.hpp>
#include <bee/utility/path_helper.h>

#if defined(__cpp_lib_char8_t)
    static std::string_view tostrview(std::u8string const& u8str) {
        return { reinterpret_cast<const char*>(u8str.data()), u8str.size() };
    }
#else
    static std::string_view tostrview(std::string const& u8str) {
        return { u8str.data(), u8str.size() };
    }
#endif

void pushprogdir(lua_State *L) {
    auto r = bee::path_helper::exe_path();
    if (!r) {
        luaL_error(L, "unable to get progdir: %s\n", r.error().c_str());
        return;
    }
    auto u8str = r.value().remove_filename().generic_u8string();
    auto str = tostrview(u8str);
    lua_pushlstring(L, str.data(), str.size());
}
