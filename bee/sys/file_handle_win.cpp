#include <Windows.h>
#include <bee/nonstd/unreachable.h>
#include <bee/sys/file_handle.h>
#include <bee/utility/dynarray.h>
#include <fcntl.h>
#include <io.h>

namespace bee {
    static FILE* handletofile(HANDLE h, int flags, const char* mode) noexcept {
        const int fn = _open_osfhandle((intptr_t)h, flags);
        if (fn == -1) {
            return 0;
        }
        return _fdopen(fn, mode);
    }

    FILE* file_handle::to_file(mode mode) const noexcept {
        switch (mode) {
        case mode::read:
            return handletofile(h, _O_RDONLY | _O_BINARY, "rb");
        case mode::write:
            return handletofile(h, _O_WRONLY | _O_BINARY, "wb");
        default:
            std::unreachable();
        }
    }

    file_handle file_handle::from_file(FILE* f) noexcept {
        const int n = _fileno(f);
        if (n < 0) {
            return {};
        }
        return { from_native((HANDLE)_get_osfhandle(n)) };
    }

    file_handle file_handle::dup(FILE* f) noexcept {
        const file_handle h = from_file(f);
        if (!h) {
            return {};
        }
        file_handle newh;
        if (!::DuplicateHandle(::GetCurrentProcess(), h.value(), ::GetCurrentProcess(), &newh, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
            return {};
        }
        return newh;
    }

    file_handle file_handle::lock(path_view filename) noexcept {
        const HANDLE h = CreateFileW(filename.data(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE, NULL);
        return from_native(h);
    }

    file_handle file_handle::open_link(path_view filename) noexcept {
        const HANDLE h = CreateFileW(filename.data(), 0, 0, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
        return from_native(h);
    }

    void file_handle::close() noexcept {
        if (valid()) {
            CloseHandle(h);
            h = file_handle {}.h;
        }
    }

    std::optional<fs::path> file_handle::path() const {
        if (!valid()) {
            return std::nullopt;
        }
        const DWORD len = GetFinalPathNameByHandleW(h, NULL, 0, VOLUME_NAME_DOS);
        if (len == 0) {
            return std::nullopt;
        }
        dynarray<wchar_t> path(static_cast<size_t>(len));
        const DWORD len2 = GetFinalPathNameByHandleW(h, path.data(), len, VOLUME_NAME_DOS);
        if (len2 == 0 || len2 >= len) {
            return std::nullopt;
        }
        if (path[0] == L'\\' && path[1] == L'\\' && path[2] == L'?' && path[3] == L'\\') {
            // Turn the \\?\UNC\ network path prefix into \\.
            if (path[4] == L'U' && path[5] == L'N' && path[6] == L'C' && path[7] == L'\\') {
                return fs::path(L"\\\\").concat(path.data() + 8, path.data() + len2);
            }
            // Remove the \\?\ prefix.
            return fs::path { path.data() + 4, path.data() + len2 };
        }
        return fs::path { path.data(), path.data() + len2 };
    }
}
