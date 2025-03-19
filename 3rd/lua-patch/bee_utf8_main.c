#if defined(_WIN32)

#    include <Windows.h>
#    include <bee/win/cwtf8.h>
#    include <stdlib.h>
#    include <string.h>

static char** utf8_create_args(int argc, wchar_t** wargv) {
    char** argv = (char**)malloc((argc + 1) * sizeof(char*));
    if (!argv) {
        return NULL;
    }
    for (int i = 0; i < argc; ++i) {
        size_t wlen = wcslen(wargv[i]);
        size_t len  = wtf8_from_utf16_length(wargv[i], wlen);
        argv[i]     = (char*)malloc((len + 1) * sizeof(char));

        if (!argv[i]) {
            for (int j = 0; j < i; ++j) {
                free(argv[j]);
            }
            free(argv);
            return NULL;
        }
        wtf8_from_utf16(wargv[i], wlen, argv[i], len);
        argv[i][len] = '\0';
    }
    argv[argc] = NULL;
    return argv;
}

static void utf8_free_args(int argc, char** argv) {
    for (int i = 0; i < argc; ++i) {
        free(argv[i]);
    }
    free(argv);
}

extern int utf8_main(int argc, char** argv);

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

int wmain(int argc, wchar_t** wargv) {
    enable_vtmode(GetStdHandle(STD_OUTPUT_HANDLE));
    enable_vtmode(GetStdHandle(STD_ERROR_HANDLE));
    char** argv = utf8_create_args(argc, wargv);
    if (!argv) {
        return EXIT_FAILURE;
    }
    int ret = utf8_main(argc, argv);
    utf8_free_args(argc, argv);
    return ret;
}

#    if defined(__MINGW32__)

extern int _CRT_glob;
extern void __wgetmainargs(int*, wchar_t***, wchar_t***, int, int*);

int main() {
    wchar_t **enpv, **argv;
    int argc, si = 0;
    __wgetmainargs(&argc, &argv, &enpv, _CRT_glob, &si);
    return wmain(argc, argv);
}

#    endif

#endif
