#pragma once

#if LUA_VERSION_NUM >= 505
#    if !defined(lua_writestringerror)
#        if defined(_WIN32)
#            include "bee_utf8_crt.h"
#            define lua_writestringerror(s, p) utf8_ConsoleError(s, p)
#        else
#            define lua_writestringerror(s, p) (fprintf(stderr, (s), (p)), fflush(stderr))
#        endif
#    endif
#endif
