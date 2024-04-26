local lm = require "luamake"

lm:lua_dll "bee" {
    deps = "source_bee",
    windows = {
        export_luaopen = "off"
    },
}

if lm.os == "windows" then
    lm:shared_library "lua54" {
        deps = "bee_utf8_crt",
        sources = {
            "3rd/lua/onelua.c",
            "3rd/lua/linit.c",
        },
        defines = {
            "MAKE_LIB",
            "LUA_BUILD_AS_DLL",
        },
        msvc = {
            flags = "/wd4334",
            sources = ("3rd/lua/fast_setjmp_%s.s"):format(lm.arch)
        }
    }
    lm:executable "lua" {
        deps = {
            "lua54",
            "bee_utf8_crt",
        },
        includes = ".",
        sources = {
            "3rd/lua/bee_lua.c",
            "3rd/lua/bee_utf8_main.c",
        }
    }
    lm:executable "luac" {
        deps = "bee_utf8_crt",
        includes = ".",
        sources = {
            "3rd/lua/onelua.c",
            "3rd/lua/bee_utf8_main.c",
        },
        defines = {
            "MAKE_LUAC",
        },
        msvc = {
            flags = "/wd4334",
            sources = ("3rd/lua/fast_setjmp_%s.s"):format(lm.arch),
        }
    }
    return
end

lm:executable "lua" {
    deps = "source_lua",
    sources = "3rd/lua/lua.c",
    macos = {
        defines = "LUA_USE_MACOSX",
        links = { "m", "dl" },
    },
    linux = {
        defines = "LUA_USE_LINUX",
        links = { "m", "dl" }
    },
    netbsd = {
        defines = "LUA_USE_LINUX",
        links = "m",
    },
    freebsd = {
        defines = "LUA_USE_LINUX",
        links = "m",
    },
    openbsd = {
        defines = "LUA_USE_LINUX",
        links = "m",
    },
    android = {
        defines = "LUA_USE_LINUX",
        links = { "m", "dl" },
    }
}
