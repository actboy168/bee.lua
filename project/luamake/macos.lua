local lm = require 'luamake'

lm.rootdir = '3rd/lua/src'
lm:executable 'lua' {
    sources = {
        "*.c",
        "!luac.c",
    },
    defines = {
        "LUA_USE_MACOSX",
    },
    links = { "m", "dl" },
}

lm.rootdir = ''

lm:shared_library 'bee' {
    includes = {
        "3rd/lua/src",
        "3rd/lua-seri",
        "."
    },
    defines = {
        "span_FEATURE_BYTE_SPAN=1"
    },
    sources = {
        "3rd/lua-seri/*.c",
        "bee/*.cpp",
        "binding/*.cpp",
        "!bee/*_win.cpp",
        "!bee/*_linux.cpp",
        "!binding/lua_unicode.cpp",
        "!binding/lua_registry.cpp",
    },
    ldflags = {
        "-framework CoreFoundation",
        "-framework CoreServices",
    }
}

lm:executable 'bootstrap' {
    includes = {
        "3rd/lua/src"
    },
    sources = {
        "3rd/lua/src/*.c",
        "!3rd/lua/src/lua.c",
        "!3rd/lua/src/luac.c",
        "bootstrap/*.cpp",
    },
    flags = {
        "-fvisibility=hidden",
    },
    defines = {
        "LUA_USE_MACOSX",
    },
    links = { "m", "dl" },
}

lm:build "copy_script" {
    "mkdir", "-p", "$bin", "&&",
    "cp", "bootstrap/main.lua", "$bin/main.lua"
}

lm:build "test" {
    "$bin/bootstrap", "test/test.lua",
    deps = { "bootstrap", "copy_script", "bee" },
}
