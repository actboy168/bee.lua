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
    defines = {
        "LUA_USE_MACOSX",
    },
    visibility = "default",
}

lm:executable 'lua' {
    deps = "source_lua",
    sources = {
        "3rd/lua/lua.c",
    },
    defines = {
        "LUA_USE_MACOSX",
    },
    links = { "m", "dl" },
}

lm:shared_library 'bee' {
    includes = {
        "3rd/lua",
        "3rd/lua-seri",
        "bee/nonstd",
        "."
    },
    sources = {
        "3rd/lua-seri/*.c",
        "bee/*.cpp",
        "bee/nonstd/fmt/*.cc",
        "binding/*.cpp",
        "!bee/*_win.cpp",
        "!bee/*_linux.cpp",
        "!binding/lua_unicode.cpp",
        "!binding/lua_registry.cpp",
        "!binding/lua_wmi.cpp",
    },
    ldflags = {
        "-framework CoreFoundation",
        "-framework CoreServices",
    }
}

lm:executable 'bootstrap' {
    deps = "source_lua",
    includes = {
        "3rd/lua"
    },
    sources = {
        "bootstrap/*.cpp",
    },
    defines = {
        "LUA_USE_MACOSX",
    },
    links = { "m", "dl" },
}
