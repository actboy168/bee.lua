local lm = require "luamake"
lm.rootdir = ".."

require 'config'
lm:import 'common.lua'

local BOOTSTRAP = lm.EXE_NAME or "bootstrap"
local EXE_DIR = lm.EXE_DIR or "$bin"

lm:source_set "source_bootstrap" {
    deps = { "source_bee", "source_lua" },
    includes = {"3rd/lua", "."},
    sources = "bootstrap/*.cpp",
    windows = {
        sources = "3rd/lua/utf8_crt.c",
    },
    macos = {
        defines = "LUA_USE_MACOSX",
        links = { "m", "dl" },
    },
    linux = {
        defines = "LUA_USE_LINUX",
        ldflags = {
            "-Wl,-E",
        },
        links = {
            "m", "dl",
        }
    },
    android = {
        defines = "LUA_USE_LINUX",
        ldflags = "-Wl,-E",
        links = { "m", "dl" }
    }
}

lm:executable (BOOTSTRAP) {
    bindir = lm.EXE_DIR,
    deps = "source_bootstrap",
    windows = {
        sources = {
            lm.EXE_RESOURCE,
        }
    },
    msvc = {
        ldflags = "/IMPLIB:$obj/"..BOOTSTRAP..".lib"
    },
    mingw = {
        ldflags = "-Wl,--out-implib,$obj/"..BOOTSTRAP..".lib"
    },
    linux = {
        crt = "static",
    },
}

local exe = lm.os == 'windows' and ".exe" or ""

lm:copy "copy_script" {
    input = "bootstrap/main.lua",
    output = EXE_DIR.."/main.lua",
    deps = BOOTSTRAP,
}

if not lm.notest then
    local tests = {}
    local fs = require "bee.filesystem"
    for file in fs.pairs(lm.workdir .. "/test", "r") do
        if file:equal_extension ".lua" then
            tests[#tests+1] = file
        end
    end
    lm:build "test" {
        EXE_DIR.."/"..BOOTSTRAP..exe, "@test/test.lua",
        "--touch", "$out",
        deps = { BOOTSTRAP, "copy_script" },
        description = "Run test.",
        input = tests,
        output = "$obj/test.stamp",
        pool = "console",
    }
end

if lm.os == "windows" then
    lm:build "forward_lua" {
        "$luamake", "lua",
        "@bootstrap/forward_lua.lua",
        "@3rd/lua/", "$out",
        BOOTSTRAP..".exe", lm.compiler,
        input = {
            "bootstrap/forward_lua.lua",
            "3rd/lua/lua.h",
            "3rd/lua/lauxlib.h",
            "3rd/lua/lualib.h",
        },
        output = "bootstrap/forward_lua.h",
        deps = {
            lm.LUAMAKE,
        }
    }
    lm:phony {
        input = "bootstrap/forward_lua.h",
        output = "bootstrap/forward_lua.c",
    }
    lm:shared_library "lua54" {
        bindir = lm.EXE_DIR,
        includes = "bootstrap",
        sources = "bootstrap/forward_lua.c",
        ldflags = "$obj/"..BOOTSTRAP..".lib",
        deps = {
            "forward_lua",
            BOOTSTRAP,
        }
    }
end
