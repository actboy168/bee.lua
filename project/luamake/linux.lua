local lm = require 'luamake'

lm.rootdir = '3rd/lua/'
lm:executable 'lua' {
    sources = {
        "*.c",
        "!luac.c",
    },
    ldflags = "-Wl,-E",
    defines = "LUA_USE_LINUX",
    links = { "m", "dl" },
}
