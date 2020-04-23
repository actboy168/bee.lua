local lm = require 'luamake'

lm:shared_library 'lua54' {
    rootdir = '3rd/lua/src',
    sources = {
        "*.c",
        "!lua.c",
        "!luac.c",
        "../utf8/utf8_crt.c",
    },
    defines = {
        "LUA_BUILD_AS_DLL",
    }
}

lm:executable 'lua' {
    rootdir = '3rd/lua/src',
    deps = "lua54",
    sources = {
        "../utf8/utf8_lua.c",
    }
}

lm:shared_library 'bee' {
    deps = "lua54",
    includes = {
        "3rd/lua/src",
        "3rd/lua-seri",
        "."
    },
    defines = {
        "_WIN32_WINNT=0x0601",
        "BEE_EXPORTS",
        "_CRT_SECURE_NO_WARNINGS",
        "span_FEATURE_BYTE_SPAN=1",
    },
    sources = {
        "3rd/lua-seri/*.c",
        "bee/*.cpp",
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
        lm.plat == "mingw" and "stdc++fs",
        lm.plat == "mingw" and "stdc++"
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
