#include <Windows.h>
#include <bee/error.h>
#include <bee/lua/binding.h>
#include <bee/lua/error.h>
#include <bee/platform/win/unicode.h>
#include <bee/platform/win/wtf8.h>
#include <binding/binding.h>
#include <binding/file.h>
#include <fcntl.h>
#include <io.h>

namespace bee::lua_windows {
    static int lu2a(lua_State* L) {
        auto r = win::u2a(lua::checkstrview(L, 1));
        lua_pushlstring(L, r.data(), r.size());
        return 1;
    }

    static int la2u(lua_State* L) {
        auto r = win::a2u(lua::checkstrview(L, 1));
        lua_pushlstring(L, r.data(), r.size());
        return 1;
    }

    static int filemode(lua_State* L) {
        luaL_Stream* p = lua::tofile(L, 1);
        auto mode      = lua::checkstrview(L, 2);
        if (!p || !p->closef || !p->f) {
            return lua::push_error(L, error::crt_errmsg("filemode", std::errc::bad_file_descriptor));
        }
        int ok = _setmode(_fileno(p->f), mode[0] == 'b' ? _O_BINARY : _O_TEXT);
        if (ok == -1) {
            return lua::push_error(L, error::crt_errmsg("filemode"));
        }
        lua_pushboolean(L, 1);
        return 1;
    }

    static int isatty(lua_State* L) {
        luaL_Stream* p = lua::tofile(L, 1);
        if (!p || !p->closef || !p->f) {
            lua_pushboolean(L, 0);
            return 1;
        }
        HANDLE handle = (HANDLE)_get_osfhandle(_fileno(p->f));
        lua_pushboolean(L, FILE_TYPE_CHAR == GetFileType(handle));
        return 1;
    }

    static int write_console(lua_State* L) {
        luaL_Stream* p = lua::tofile(L, 1);
        auto msg       = wtf8::u2w(lua::checkstrview(L, 2));
        if (!p || !p->closef || !p->f) {
            return lua::push_error(L, error::crt_errmsg("write_console", std::errc::bad_file_descriptor));
        }
        HANDLE handle = (HANDLE)_get_osfhandle(_fileno(p->f));
        DWORD written = 0;
        BOOL ok       = WriteConsoleW(handle, (void*)msg.c_str(), (DWORD)msg.size(), &written, NULL);
        if (!ok) {
            return lua::push_error(L, error::sys_errmsg("write_console"));
        }
        lua_pushinteger(L, written);
        return 1;
    }

    static int luaopen(lua_State* L) {
        luaL_Reg lib[] = {
            { "u2a", lu2a },
            { "a2u", la2u },
            { "filemode", filemode },
            { "isatty", isatty },
            { "write_console", write_console },
            { NULL, NULL }
        };
        luaL_newlibtable(L, lib);
        luaL_setfuncs(L, lib, 0);
        return 1;
    }
}

DEFINE_LUAOPEN(windows)
