local lm = require 'luamake'

lm:source_set 'source_lua' {
    sources = {
        "3rd/lua/*.c",
        "!3rd/lua/luac.c",
        "!3rd/lua/lua.c",
        "!3rd/lua/utf8_*.c",
    },
    defines = "LUA_USE_LINUX",
    visibility = "default",
}

lm:executable 'lua' {
    deps = "source_lua",
    sources = "3rd/lua/lua.c",
    ldflags = "-Wl,-E",
    defines = "LUA_USE_LINUX",
    links = { "m", "dl" }
}

lm:executable 'bootstrap' {
    deps = "source_lua",
    includes = "3rd/lua",
    sources = "bootstrap/*.cpp",
    ldflags = "-Wl,-E",
    defines = "LUA_USE_LINUX",
    links = { "m", "dl" }
}
