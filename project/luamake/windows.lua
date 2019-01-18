local lm = require 'luamake'

lm.rootdir = '3rd/lua/'

lm:shared_library 'lua54' {
    sources = {
        "src/*.c",
        "!src/lua.c",
        "!src/luac.c",
        "utf8/utf8_crt.c",
    },
    defines = {
        "LUA_BUILD_AS_DLL",
        "LUAI_MAXCCALLS=200"
    }
}
lm:executable 'lua' {
    deps = "lua54",
    sources = {
        "utf8/utf8_lua.c",
    }
}

lm.rootdir = ''
lm:shared_library 'bee' {
    deps = "lua54",
    includes = {
        "3rd/lua/src",
        "3rd/lua-seri",
        "3rd/incbin",
        "."
    },
    defines = {
        "_WIN32_WINNT=0x0601",
        "BEE_EXPORTS",
        "_CRT_SECURE_NO_WARNINGS"
    },
    sources = {
        "3rd/lua-seri/*.c",
        "bee/*.cpp",
        "binding/*.cpp",
        "!bee/fsevent/fsevent_osx.cpp",
        "!bee/subprocess/subprocess_posix.cpp",
        "!binding/lua_posixfs.cpp",
    },
    links = {
        "advapi32",
        "ws2_32",
        "version",
        "stdc++fs",
        "stdc++"
    }
}

lm:executable 'bootstrap' {
    deps = "lua54",
    includes = {
        "3rd/lua/src"
    },
    sources = {
        "bootstrap/*.cpp",
    },
}

lm:build "copy_script" {
    "$luamake", "lua", "project/luamake/copy.lua", "bootstrap/main.lua", "$bin/main.lua"
}

lm:build "test" {
    "$bin/bootstrap.exe", "test/test.lua",
    deps = { "bootstrap", "copy_script", "bee" },
}
