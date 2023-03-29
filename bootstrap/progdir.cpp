#include <bee/utility/path_helper.h>

#include <lua.hpp>
#include <string_view>

template <typename CharT>
static std::string_view tostrview(std::basic_string<CharT> const& u8str) {
    static_assert(sizeof(CharT) == sizeof(char));
    return { reinterpret_cast<const char*>(u8str.data()), u8str.size() };
}

void pushprogdir(lua_State* L) {
    auto r = bee::path_helper::exe_path();
    if (!r) {
        luaL_error(L, "unable to get progdir: %s\n", r.error().c_str());
        return;
    }
    auto u8str = r.value().remove_filename().generic_u8string();
    auto str   = tostrview(u8str);
    lua_pushlstring(L, str.data(), str.size());
}
