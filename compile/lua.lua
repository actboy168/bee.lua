local lm = require "luamake"

if lm.os == "windows" then
    lm:shared_library("lua"..lm.lua) {
        deps = lm.optchain and { "bee_utf8_crt", "apply_optchain_patch" } or "bee_utf8_crt",
        includes = lm.optchain and { lm.luadir, lm:path("3rd/lua"..lm.lua) } or lm.luadir,
        sources = {
            lm.luadir / "onelua.c",
            lm.luadir / "linit.c",
        },
        defines = lm.optchain and {
            "MAKE_LIB",
            "LUA_BUILD_AS_DLL",
            "BEE_OPTCHAIN",
        } or {
            "MAKE_LIB",
            "LUA_BUILD_AS_DLL",
        },
        msvc = lm.fast_setjmp ~= "off" and {
            defines = "BEE_FAST_SETJMP",
            sources = ("3rd/lua-patch/fast_setjmp_%s.s"):format(lm.arch),
        }
    }
    lm:executable "lua" {
        deps = lm.optchain and {
            "bee_utf8_crt",
            "lua"..lm.lua,
            "apply_optchain_patch",
        } or {
            "bee_utf8_crt",
            "lua"..lm.lua,
        },
        includes = lm.optchain and {
            ".",
            lm.luadir,
            lm:path("3rd/lua"..lm.lua),
        } or {
            ".",
            lm.luadir,
        },
        sources = {
            "3rd/lua-patch/bee_lua.c",
            "3rd/lua-patch/bee_assert.c",
            "3rd/lua-patch/bee_utf8_main.c",
        }
    }
    lm:executable "luac" {
        deps = lm.optchain and { "bee_utf8_crt", "apply_optchain_patch" } or "bee_utf8_crt",
        includes = lm.optchain and { ".", lm.luadir, lm:path("3rd/lua"..lm.lua) } or ".",
        sources = {
            lm.luadir / "onelua.c",
            "3rd/lua-patch/bee_utf8_main.c",
        },
        defines = lm.optchain and {
            "MAKE_LUAC",
            "BEE_OPTCHAIN",
        } or {
            "MAKE_LUAC",
        },
        msvc = lm.fast_setjmp ~= "off" and {
            defines = "BEE_FAST_SETJMP",
            sources = ("3rd/lua-patch/fast_setjmp_%s.s"):format(lm.arch),
        }
    }
    lm:shared_library "bee" {
        deps = {
            "source_bee",
            "lua"..lm.lua,
            "bee_utf8_crt",
        },
        windows = {
            export_luaopen = "off"
        },
    }
    return
end

lm:executable "lua" {
    deps = "source_lua",
    includes = lm.optchain and { lm.luadir, lm:path("3rd/lua"..lm.lua) } or lm.luadir,
    sources = {
        lm.luadir / "lua.c",
        lm.luadir / "linit.c",
    },
    visibility = "default",
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

lm:shared_library "bee" {
    deps = {
        "source_bee",
        "lua",
    },
    windows = {
        export_luaopen = "off"
    },
}
