local lm = require 'luamake'

lm.rootdir = '3rd/lua/'
lm:executable 'lua' {
    sources = {
        "src/*.c",
        "!src/luac.c",
    },
    defines = "LUA_USE_MACOSX",
    links = { "m", "dl" },
}
