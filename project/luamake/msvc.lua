local lm = require 'luamake'

lm.defines = {
    "_WIN32_WINNT=0x0601",
}

lm:shared_library 'lua54' {
    rootdir = '3rd/lua',
    sources = {
        "*.c",
        "!lua.c",
        "!luac.c",
        "!utf8_lua.c",
    },
    defines = {
        "LUA_BUILD_AS_DLL",
    }
}

lm:executable 'lua' {
    rootdir = '3rd/lua',
    deps = "lua54",
    sources = {
        "utf8_lua.c",
        "utf8_unicode.c",
    }
}

lm:shared_library 'bee' {
    deps = "lua54",
    includes = {
        "3rd/lua",
        "3rd/lua-seri",
        "bee/nonstd",
        "."
    },
    defines = {
        "BEE_EXPORTS",
        "_CRT_SECURE_NO_WARNINGS",
    },
    sources = {
        "3rd/lua-seri/*.c",
        "bee/*.cpp",
        "bee/nonstd/fmt/*.cc",
        "binding/*.cpp",
        "!bee/*_osx.cpp",
        "!bee/*_linux.cpp",
        "!bee/*_posix.cpp",
    },
    links = {
        "advapi32",
        "ws2_32",
        "ole32",
        "user32",
        "version",
        "wbemuuid",
        "oleAut32",
        lm.plat == "mingw" and "stdc++fs",
        lm.plat == "mingw" and "stdc++"
    }
}

lm:executable 'bootstrap' {
    deps = "lua54",
    includes = {
        "3rd/lua"
    },
    sources = {
        "bootstrap/*.cpp",
    }
}

if lm.plat == 'msvc' then
    lm:build "copy_script" {
        "cmd.exe", "/C", "@project/copy.bat", "@bootstrap/main.lua", "build\\msvc\\bin"
    }
else
    lm:build "copy_script" {
        "mkdir", "-p", "$bin", "&&",
        "cp", "@bootstrap/main.lua", "$bin/main.lua"
    }
end

lm:build "test" {
    "$bin/bootstrap.exe", "@test/test.lua",
    deps = { "bootstrap", "copy_script", "bee" },
    pool = "console"
}
