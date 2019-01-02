#pragma once

#include <stdio.h>
#if defined(_WIN32)
#include <Windows.h>
#endif

namespace bee::file {
#if defined(_WIN32)
    typedef HANDLE handle;
#else
    typedef int handle;
#endif
    enum class mode {
        eRead,
        eWrite,
    };
    FILE*  open(handle h, mode m);
    handle get_handle(FILE* f);
    handle dup(FILE* f);
}
