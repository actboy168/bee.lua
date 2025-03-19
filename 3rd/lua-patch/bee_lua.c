#if defined(_WIN32)
#    define main(a, b) utf8_main(a, b)
#    include "lua.c"
#    undef main
#else
#    include "lua.c"
#endif
