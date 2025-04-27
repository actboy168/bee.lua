local lm = require "luamake"

if lm.os == "windows" then
    lm:shared_library(lm.lua == "55" and "lua55" or "lua54") {
        deps = "bee_utf8_crt",
        sources = {
            lm.luadir / "onelua.c",
            lm.luadir / "linit.c",
        },
        defines = {
            "MAKE_LIB",
            "LUA_BUILD_AS_DLL",
        },
        msvc = lm.fast_setjmp ~= "off" and {
            defines = "BEE_FAST_SETJMP",
            sources = ("3rd/lua-patch/fast_setjmp_%s.s"):format(lm.arch),
        }
    }
    lm:executable "lua" {
        deps = {
            "bee_utf8_crt",
            lm.lua == "55" and "lua55" or "lua54",
        },
        includes = {
            ".",
            lm.luadir,
        },
        sources = {
            "3rd/lua-patch/bee_lua.c",
            "3rd/lua-patch/bee_utf8_main.c",
        }
    }
    if lm.lua ~= "55" then
        ---@TODO lua55's luac is not working
        lm:executable "luac" {
            deps = "bee_utf8_crt",
            includes = ".",
            sources = {
                lm.luadir / "onelua.c",
                "3rd/lua-patch/bee_utf8_main.c",
            },
            defines = {
                "MAKE_LUAC",
            },
            msvc = lm.fast_setjmp ~= "off" and {
                defines = "BEE_FAST_SETJMP",
                sources = ("3rd/lua-patch/fast_setjmp_%s.s"):format(lm.arch),
            }
        }
    end
    return
end

lm:shared_library "bee" {
    deps = {
        "source_bee",
        lm.lua == "55" and "lua55" or "lua54",
    },
    windows = {
        export_luaopen = "off"
    },
}

lm:executable "lua" {
    deps = "source_lua",
    sources = lm.luadir / "lua.c",
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
