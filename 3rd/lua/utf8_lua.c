#if !defined(_WIN32)

#    include "lua.c"

#else

#    define main(a, b) utf8_main(a, b)
#    include "lua.c"
#    undef main

#    include <wchar.h>

static void enable_vtmode(HANDLE h) {
    if (h == INVALID_HANDLE_VALUE) {
        return;
    }
    DWORD mode = 0;
    if (!GetConsoleMode(h, &mode)) {
        return;
    }
    mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(h, mode);
}

int wmain(int argc, wchar_t **wargv) {
    enable_vtmode(GetStdHandle(STD_OUTPUT_HANDLE));
    enable_vtmode(GetStdHandle(STD_ERROR_HANDLE));
    char **argv = utf8_create_args(argc, wargv);
    if (!argv) {
        return EXIT_FAILURE;
    }
    int ret = utf8_main(argc, argv);
    utf8_free_args(argc, argv);
    return ret;
}

#    if defined(__MINGW32__)

#        include <stdlib.h>

extern int _CRT_glob;
extern
#        ifdef __cplusplus
    "C"
#        endif
    void
    __wgetmainargs(int *, wchar_t ***, wchar_t ***, int, int *);

int main() {
    wchar_t **enpv, **argv;
    int argc, si = 0;
    __wgetmainargs(&argc, &argv, &enpv, _CRT_glob, &si);
    return wmain(argc, argv);
}

#    endif

#endif
