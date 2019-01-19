local lm = require 'luamake'

lm.rootdir = '3rd/lua/'
lm:executable 'lua' {
    sources = {
        "src/*.c",
        "!src/luac.c",
    },
    ldflags = "-Wl,-E",
    defines = "LUA_USE_LINUX",
    links = { "m", "dl" },
}

lm.rootdir = ''

lm:shared_library 'bee' {
    includes = {
        "3rd/lua/src",
        "3rd/lua-seri",
        "3rd/incbin",
        "."
    },
    defines = {
    },
    sources = {
        "3rd/lua-seri/*.c",
        "bee/*.cpp",
        "binding/*.cpp",
        "!bee/error/exception.cpp",
        "!bee/utility/unicode.cpp",
        "!bee/net/unixsocket.cpp",
        "!bee/error/windows_category.cpp",
        "!bee/utility/module_version.cpp",
        "!bee/platform/version.cpp",
        "!bee/*_win.cpp",
        "!bee/*_osx.cpp",
        "!binding/lua_posixfs.cpp",
        "!binding/lua_filewatch.cpp",
        "!binding/lua_registry.cpp",
    },
    links = {
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
        "!3rd/lua/src/luac.c",
        "bootstrap/*.cpp",
    },
}

lm:build "copy_script" {
    "cp", "bootstrap/main.lua", "$bin/main.lua"
}

lm:build "test" {
    "$bin/bootstrap", "test/test.lua",
    deps = { "bootstrap", "copy_script", "bee" },
}
