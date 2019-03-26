local lm = require 'luamake'

lm.rootdir = '3rd/lua/'
lm:executable 'lua' {
    sources = {
        "src/*.c",
        "!src/luac.c",
    },
    defines = {
        "LUA_USE_MACOSX",
        ("LUAI_MAXCSTACK=%d"):format(LUAI_MAXCSTACK)
    },
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
    defines = {
        "LUA_USE_MACOSX",
        "LUAI_MAXCCALLS=200"
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
