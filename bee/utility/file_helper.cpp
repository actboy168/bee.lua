#include <bee/utility/file_helper.h>
#include <assert.h>
#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#else
#include <unistd.h>
#endif

namespace bee::file {

#if defined(_WIN32)
    FILE* open(handle h, mode m) {
        switch (m) {
        case mode::eRead:
            return _fdopen(_open_osfhandle((intptr_t)h, _O_RDONLY | _O_BINARY), "rb");
        case mode::eWrite:
            return _fdopen(_open_osfhandle((intptr_t)h, _O_WRONLY | _O_BINARY), "wb");
        default:
            assert(false);
            return 0;
        }
    }

    handle get_handle(FILE* f) {
        int n = _fileno(f);
        return (n >= 0) ? (HANDLE)_get_osfhandle(n) : INVALID_HANDLE_VALUE;
    }

    handle dup(FILE* f) {
        handle h = get_handle(f);
        if (h == INVALID_HANDLE_VALUE) {
            return 0;
        }
        handle newh;
        if (!::DuplicateHandle(::GetCurrentProcess(), h, ::GetCurrentProcess(), &newh, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
            return 0;
        }
        return newh;
    }
#else
    FILE* open(handle h, mode m) {
        switch (m) {
        case mode::eRead:
            return fdopen(h, "rb");
        case mode::eWrite:
            return fdopen(h, "wb");
        default:
            assert(false);
            return 0;
        }
    }

    handle get_handle(FILE* f) {
        return fileno(f);
    }

    handle dup(FILE* f) {
        return ::dup(get_handle(f));
    }
#endif
}
