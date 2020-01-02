local lm = require 'luamake'

lm.gcc = 'clang'
lm.gxx = 'clang++'

lm.rootdir = '3rd/lua/src'
lm:executable 'lua' {
    sources = {
        "*.c",
        "!luac.c",
    },
    ldflags = "-Wl,-E",
    defines = {
        "LUA_USE_LINUX",
    },
    visibility = "default",
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
        "!bee/*_osx.cpp",
        "!binding/lua_unicode.cpp",
        "!binding/lua_registry.cpp",
    },
    links = {
        "pthread",
        "stdc++fs",
        "stdc++"
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
    ldflags = "-Wl,-E",
    defines = {
        "LUA_USE_LINUX",
    },
    visibility = "default",
    links = { "m", "dl" },
}

local fs = require "bee.filesystem"

lm:build "copy_script" {
    "mkdir", "-p", "$bin", "&&",
    "cp", fs.path "bootstrap/main.lua", "$bin/main.lua"
}

lm:build "test" {
    "$bin/bootstrap", fs.path "test/test.lua",
    deps = { "bootstrap", "copy_script", "bee" },
}
