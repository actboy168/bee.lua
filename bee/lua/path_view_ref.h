#pragma once

#include <bee/lua/binding.h>
#include <bee/lua/udata.h>
#include <bee/nonstd/filesystem.h>
#include <bee/utility/path_view.h>

#if defined(_WIN32)
#    include <bee/win/wtf8.h>
#endif

#include <lua.hpp>

namespace bee {
    class path_view_ref {
    public:
        path_view_ref() noexcept = default;
        path_view_ref(lua_State* L, int idx) {
            if (lua_type(L, idx) == LUA_TSTRING) {
#if defined(_WIN32)
                str  = wtf8::u2w(lua::checkstrview(L, idx));
                view = str;
#else
                view = lua::checkstrview(L, idx);
#endif
            } else {
                view = lua::checkudata<fs::path>(L, idx).native();
            }
        }
        operator path_view() const noexcept {
            return view;
        }

    private:
#if defined(_WIN32)
        std::wstring str;
#endif
        path_view view;
    };
}
