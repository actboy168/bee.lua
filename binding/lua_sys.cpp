#include <bee/lua/binding.h>
#include <bee/lua/error.h>
#include <bee/lua/file.h>
#include <bee/lua/module.h>
#include <bee/lua/udata.h>
#include <bee/nonstd/filesystem.h>
#include <bee/sys/file_handle.h>
#include <bee/sys/path.h>

#if defined(_WIN32)
#    include <Windows.h>
#    include <bee/win/wtf8.h>
#endif

namespace bee::lua {
    fs::path& new_path(lua_State* L, fs::path&& path);
}

namespace bee::lua_sys {
    class path_view_ref {
    public:
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

    static int exe_path(lua_State* L) {
        auto r = sys::exe_path();
        if (!r) {
            return lua::return_sys_error(L, "exe_path");
        }
        lua::new_path(L, std::move(*r));
        return 1;
    }

    static int dll_path(lua_State* L) {
        auto r = sys::dll_path();
        if (!r) {
            return lua::return_sys_error(L, "dll_path");
        }
        lua::new_path(L, std::move(*r));
        return 1;
    }

    static int filelock(lua_State* L) {
        path_view_ref path(L, 1);
        auto fd = file_handle::lock(path);
        if (!fd) {
            return lua::return_sys_error(L, "filelock");
        }
        auto f = fd.to_file(file_handle::mode::write);
        if (!f) {
            int r = lua::return_crt_error(L, "filelock");
            fd.close();
            return r;
        }
        lua::newfile(L, f);
        return 1;
    }

    static int fullpath(lua_State* L) {
        path_view_ref path(L, 1);
        auto fd = file_handle::open_link(path);
        if (!fd) {
            return lua::return_sys_error(L, "fullpath");
        }
        auto fullpath = fd.path();
        fd.close();
        if (!fullpath) {
            int r = lua::return_sys_error(L, "fullpath");
            fd.close();
            return r;
        }
        fd.close();
        lua::new_path(L, std::move(*fullpath));
        return 1;
    }

    static int luaopen(lua_State* L) {
        static luaL_Reg lib[] = {
            { "exe_path", exe_path },
            { "dll_path", dll_path },
            { "filelock", filelock },
            { "fullpath", fullpath },
            { NULL, NULL },
        };
        luaL_newlibtable(L, lib);
        luaL_setfuncs(L, lib, 0);

        return 1;
    }
}

DEFINE_LUAOPEN(sys)
