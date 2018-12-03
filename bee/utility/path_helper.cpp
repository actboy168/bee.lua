#include <bee/utility/path_helper.h>
#include <bee/utility/dynarray.h>
#include <bee/error.h>
#include <Windows.h>

// http://blogs.msdn.com/oldnewthing/archive/2004/10/25/247180.aspx
extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace bee::path_helper {
    auto module(HMODULE module_handle) ->nonstd::expected<fs::path, std::exception> {
        wchar_t buffer[MAX_PATH];
        DWORD path_len = ::GetModuleFileNameW(module_handle, buffer, _countof(buffer));
        if (path_len == 0) {
            return nonstd::make_unexpected(make_syserror("GetModuleFileNameW"));
        }
        if (path_len < _countof(buffer)) {
            return std::move(fs::path(buffer, buffer + path_len));
        }
        for (size_t buf_len = 0x200; buf_len <= 0x10000; buf_len <<= 1) {
            std::dynarray<wchar_t> buf(path_len);
            path_len = ::GetModuleFileNameW(module_handle, buf.data(), buf.size());
            if (path_len == 0) {
                return nonstd::make_unexpected(make_syserror("GetModuleFileNameW"));
            }
            if (path_len < _countof(buffer)) {
                return std::move(fs::path(buf.begin(), buf.end()));
            }
        }
        return nonstd::make_unexpected(std::exception("::GetModuleFileNameW return too long."));
    }

    auto exe_path()->nonstd::expected<fs::path, std::exception> {
        return module(NULL);
    }

    auto dll_path()->nonstd::expected<fs::path, std::exception> {
        return module(reinterpret_cast<HMODULE>(&__ImageBase));
    }

    auto dll_path(HMODULE module_handle)->nonstd::expected<fs::path, std::exception> {
        return module(module_handle);
    }

    bool equal(fs::path const& lhs, fs::path const& rhs)
    {
        fs::path lpath = fs::absolute(lhs);
        fs::path rpath = fs::absolute(rhs);
        const fs::path::value_type* l(lpath.c_str());
        const fs::path::value_type* r(rpath.c_str());
        while ((towlower(*l) == towlower(*r) || (*l == L'\\' && *r == L'/') || (*l == L'/' && *r == L'\\')) && *l)
        { 
            ++l; ++r; 
        }
        return *l == *r;
    }
}
