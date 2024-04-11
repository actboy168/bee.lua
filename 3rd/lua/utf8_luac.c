#if !defined(_WIN32)

#    include "luac.c"

#else

#    define main(a, b) utf8_main(a, b)
#    include "luac.c"
#    undef main

#    include <wchar.h>

int wmain(int argc, wchar_t **wargv) {
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
extern "C" void __wgetmainargs(int *, wchar_t ***, wchar_t ***, int, int *);

int main() {
    wchar_t **enpv, **argv;
    int argc, si = 0;
    __wgetmainargs(&argc, &argv, &enpv, _CRT_glob, &si);
    return wmain(argc, argv);
}

#    endif

#endif
