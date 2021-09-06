local lm = require 'luamake'

lm.flags = "-Wunguarded-availability"
lm.sys = "macos10.12"

lm:source_set 'source_lua' {
    sources = {
        "3rd/lua/*.c",
        "!3rd/lua/luac.c",
        "!3rd/lua/lua.c",
        "!3rd/lua/utf8_*.c",
    },
    defines = "LUA_USE_MACOSX",
    visibility = "default",
}

lm:executable 'lua' {
    deps = "source_lua",
    sources = "3rd/lua/lua.c",
    defines = "LUA_USE_MACOSX",
    links = { "m", "dl" },
}
