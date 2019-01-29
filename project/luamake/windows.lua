local lm = require 'luamake'

lm.rootdir = '3rd/lua/src'

lm:shared_library 'lua54' {
    sources = {
        "*.c",
        "!lua.c",
        "!luac.c",
        "../utf8/utf8_crt.c",
    },
    defines = {
        "LUA_BUILD_AS_DLL",
        "LUAI_MAXCCALLS=200"
    }
}

lm.rootdir = ''

lm:executable 'lua' {
    deps = "lua54",
    sources = {
        "3rd/lua/utf8/utf8_lua.c",
    }
}


if lm.plat == 'msvc' then
    lm:build "embed_make" {
        "@make/lua", "@project/embed.lua", "@bee/nonstd/embed_detail.h", "@binding/lua_embed.cpp",
        output = "bee/nonstd/embed_detail.h"
    }
    lm:phony {
        input = "bee/nonstd/embed_detail.h",
        output = "binding/lua_embed.cpp",
    }
end

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
        "!bee/*_osx.cpp",
        "!bee/*_linux.cpp",
        "!bee/*_posix.cpp",
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

if lm.plat == 'msvc' then
    local bin = lm.bindir:gsub('/', '\\')
    lm:build "copy_script" {
        "cmd.exe", "/C", "@project/copy.bat", "@bootstrap/main.lua", bin
    }
else
    lm:build "copy_script" {
        "mkdir", "-p", "@$bin", "&&",
        "cp", "@bootstrap/main.lua", "@$bin/main.lua"
    }
end

lm:build "test" {
    "@$bin/bootstrap.exe", "@test/test.lua",
    deps = { "bootstrap", "copy_script", "bee" },
    pool = "console"
}
