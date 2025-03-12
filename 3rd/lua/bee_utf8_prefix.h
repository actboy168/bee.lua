#ifndef _LUAUTF8_PREFIX_H_
#define _LUAUTF8_PREFIX_H_

#include "bee_utf8_crt.h"

#if !defined(lua_c) && !defined(luac_c)
#    if defined fopen
#        undef fopen
#    endif
#    if defined freopen
#        undef freopen
#    endif
#    if defined _popen
#        undef _popen
#    endif
#    if defined system
#        undef system
#    endif
#    if defined remove
#        undef remove
#    endif
#    if defined rename
#        undef rename
#    endif
#    if defined getenv
#        undef getenv
#    endif
#    if defined tmpnam
#        undef tmpnam
#    endif
#    define fopen(...) utf8_fopen(__VA_ARGS__)
#    define freopen(...) utf8_freopen(__VA_ARGS__)
#    define _popen(...) utf8_popen(__VA_ARGS__)
#    define system(...) utf8_system(__VA_ARGS__)
#    define remove(...) utf8_remove(__VA_ARGS__)
#    define rename(...) utf8_rename(__VA_ARGS__)
#    define getenv(...) utf8_getenv(__VA_ARGS__)
#    define tmpnam(...) utf8_tmpnam(__VA_ARGS__)

#    if defined(loadlib_c)
#        include <Windows.h>
#        define LoadLibraryExA(...) utf8_LoadLibraryExA(__VA_ARGS__)
#        define GetModuleFileNameA(...) utf8_GetModuleFileNameA(__VA_ARGS__)
#        define FormatMessageA(...) utf8_FormatMessageA(__VA_ARGS__)
#    endif
#endif

#define lua_writestring(s, l) utf8_ConsoleWrite(s, (int)l)
#define lua_writeline() utf8_ConsoleNewLine()
#define lua_writestringerror(s, p) utf8_ConsoleError(s, p)

#endif
