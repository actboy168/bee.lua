#include <Windows.h>
#include <bee/lua/binding.h>
#include <bee/lua/error.h>
#include <bee/lua/file.h>
#include <bee/lua/module.h>
#include <bee/win/file_holders.h>
#include <bee/win/unicode.h>
#include <bee/win/wtf8.h>
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
            return lua::return_error(L, "bad file descriptor");
        }
        int ok = _setmode(_fileno(p->f), mode[0] == 'b' ? _O_BINARY : _O_TEXT);
        if (ok == -1) {
            return lua::return_crt_error(L, "filemode");
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
            return lua::return_error(L, "bad file descriptor");
        }
        HANDLE handle = (HANDLE)_get_osfhandle(_fileno(p->f));
        DWORD written = 0;
        BOOL ok       = WriteConsoleW(handle, (void*)msg.c_str(), (DWORD)msg.size(), &written, NULL);
        if (!ok) {
            return lua::return_sys_error(L, "write_console");
        }
        lua_pushinteger(L, written);
        return 1;
    }

    static int is_ssd(lua_State* L) {
        const char* DriveString          = luaL_checkstring(L, 1);
        wchar_t DeviceFullpath[MAX_PATH] = L"\\\\.\\X:";
        DeviceFullpath[4]                = (wchar_t)DriveString[0];
        HANDLE Device                    = CreateFileW(DeviceFullpath, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
        if (Device == INVALID_HANDLE_VALUE) {
            return 0;
        }
        STORAGE_PROPERTY_QUERY Query {};
        Query.PropertyId = StorageDeviceSeekPenaltyProperty;
        Query.QueryType  = PropertyStandardQuery;
        DWORD Count;
        DEVICE_SEEK_PENALTY_DESCRIPTOR Result {};
        if (!DeviceIoControl(Device, IOCTL_STORAGE_QUERY_PROPERTY, &Query, sizeof(Query), &Result, sizeof(Result), &Count, nullptr)) {
            CloseHandle(Device);
            return 0;
        }
        CloseHandle(Device);
        lua_pushboolean(L, !Result.IncursSeekPenalty);
        return 1;
    }

    static int find_file_holders(lua_State* L) {
        auto filepath = wtf8::u2w(lua::checkstrview(L, 1));
        auto holders  = bee::win::find_file_holders(filepath);
        lua_createtable(L, (int)holders.size(), 0);
        int index = 1;
        for (auto pid : holders) {
            lua_pushinteger(L, pid);
            lua_rawseti(L, -2, index++);
        }
        return 1;
    }

    static int process_name(lua_State* L) {
        auto pid        = (uint32_t)luaL_checkinteger(L, 1);
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (!hProcess) {
            return 0;
        }
        WCHAR path[MAX_PATH];
        DWORD size = MAX_PATH;
        if (!QueryFullProcessImageNameW(hProcess, 0, path, &size)) {
            CloseHandle(hProcess);
            return 0;
        }
        CloseHandle(hProcess);
        std::wstring_view full(path, size);
        auto pos = full.rfind(L'\\');
        std::wstring name(pos != std::wstring_view::npos ? full.substr(pos + 1) : full);
        auto u8   = wtf8::w2u(name);
        lua_pushlstring(L, u8.data(), u8.size());
        return 1;
    }

    static int luaopen(lua_State* L) {
        luaL_Reg lib[] = {
            { "u2a", lu2a },
            { "a2u", la2u },
            { "filemode", filemode },
            { "isatty", isatty },
            { "write_console", write_console },
            { "is_ssd", is_ssd },
            { "find_file_holders", find_file_holders },
            { "process_name", process_name },
            { NULL, NULL }
        };
        luaL_newlibtable(L, lib);
        luaL_setfuncs(L, lib, 0);
        return 1;
    }
}

DEFINE_LUAOPEN(windows)
