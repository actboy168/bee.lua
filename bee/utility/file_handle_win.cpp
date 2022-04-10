#include <bee/utility/file_handle.h>
#include <bee/utility/unreachable.h>
#include <fcntl.h>
#include <io.h>

namespace bee {
    static FILE* handletofile(HANDLE h, int flags, const char* mode) {
        int fn = _open_osfhandle((intptr_t)h, flags);
        if (fn == -1) {
            return 0;
        }
        return _fdopen(fn, mode);
    }

    FILE* file_handle::to_file(mode mode) const {
        switch (mode) {
        case mode::read:
            return handletofile(h, _O_RDONLY | _O_BINARY, "rb");
        case mode::write:
            return handletofile(h, _O_WRONLY | _O_BINARY, "wb");
        default:
            unreachable();
        }
    }

    file_handle file_handle::from_file(FILE* f) {
        int n = _fileno(f);
        if (n < 0) {
            return {};
        }
        return { (HANDLE)_get_osfhandle(n) };
    }

    file_handle file_handle::dup(FILE* f) {
        file_handle h = from_file(f);
        if (!h) {
            return {};
        }
        file_handle newh;
        if (!::DuplicateHandle(::GetCurrentProcess(), h.value(), ::GetCurrentProcess(), &newh, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
            return {};
        }
        return newh;
    }

    file_handle file_handle::lock(const string_type& filename) {
        HANDLE h = CreateFileW(filename.c_str(),
            GENERIC_WRITE,
            0, NULL,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE,
            NULL
        );
        return {h};
    }
}
