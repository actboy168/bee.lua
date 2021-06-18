local lm = require 'luamake'

lm:source_set 'source_lua' {
    sources = {
        "3rd/lua/*.c",
        "!3rd/lua/luac.c",
        "!3rd/lua/lua.c",
        "!3rd/lua/utf8_*.c",
    },
    defines = {
        "LUA_USE_LINUX",
    },
    visibility = "default",
}

lm:executable 'lua' {
    deps = "source_lua",
    sources = {
        "3rd/lua/lua.c",
    },
    ldflags = "-Wl,-E",
    defines = {
        "LUA_USE_LINUX",
    },
    links = {
        "m", "dl",
        -- https://gcc.gnu.org/bugzilla/show_bug.cgi?id=67791
        "pthread",
    }
}

lm:shared_library 'bee' {
    crt = "static",
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
        "!bee/*_osx.cpp",
        "!binding/lua_unicode.cpp",
        "!binding/lua_registry.cpp",
        "!binding/lua_wmi.cpp",
    },
    links = {
        "pthread",
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
    ldflags = "-Wl,-E",
    defines = {
        "LUA_USE_LINUX",
    },
    links = {
        "m", "dl",
        -- https://gcc.gnu.org/bugzilla/show_bug.cgi?id=67791
        "pthread",
    }
}
