#include <Windows.h>
#include <bee/sys/path.h>
#include <bee/utility/dynarray.h>
#include <shlobj.h>

// http://blogs.msdn.com/oldnewthing/archive/2004/10/25/247180.aspx
extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace bee::sys {
    static std::optional<fs::path> dll_path(HMODULE module_handle) noexcept {
        wchar_t buffer[MAX_PATH];
        DWORD path_len = ::GetModuleFileNameW(module_handle, buffer, _countof(buffer));
        if (path_len == 0) {
            return std::nullopt;
        }
        if (path_len < _countof(buffer)) {
            return fs::path(buffer, buffer + path_len);
        }
        for (DWORD buf_len = 0x200; buf_len <= 0x10000; buf_len <<= 1) {
            dynarray<wchar_t> buf(buf_len);
            path_len = ::GetModuleFileNameW(module_handle, buf.data(), buf_len);
            if (path_len == 0) {
                return std::nullopt;
            }
            if (path_len < _countof(buffer)) {
                return fs::path(buf.data(), buf.data() + path_len);
            }
        }
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return std::nullopt;
    }

    std::optional<fs::path> exe_path() noexcept {
        return dll_path(NULL);
    }

    std::optional<fs::path> dll_path() noexcept {
        return dll_path(reinterpret_cast<HMODULE>(&__ImageBase));
    }
}
