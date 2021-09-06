local lm = require 'luamake'

lm.defines = {
    "_WIN32_WINNT=0x0601",
}

local STACK_SIZE = lm.mode == "debug" and lm.arch == "x86_64" and lm.compiler == "msvc"
if STACK_SIZE then
    lm.ldflags = "/STACK:"..0x160000
end

lm:shared_library 'lua54' {
    sources = {
        "3rd/lua/*.c",
        "!3rd/lua/lua.c",
        "!3rd/lua/luac.c",
        "!3rd/lua/utf8_lua.c",
    },
    defines = "LUA_BUILD_AS_DLL",
}

lm:executable 'lua' {
    deps = "lua54",
    sources = {
        "3rd/lua/utf8_lua.c",
        "3rd/lua/utf8_crt.c",
        lm.EXE_RESOURCE,
    }
}
