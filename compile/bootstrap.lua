local lm = require "luamake"
lm.rootdir = ".."

require "config"
lm:import "common.lua"

lm:source_set "source_bootstrap" {
    deps = { "source_bee", "source_lua" },
    includes = { "3rd/lua", "." },
    sources = "bootstrap/main.cpp",
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
        links = { "m", "dl" }
    }
}

lm:executable "bootstrap" {
    bindir = "$bin",
    deps = "source_bootstrap",
    msvc = {
        ldflags = "/IMPLIB:$obj/bootstrap.lib"
    },
    mingw = {
        ldflags = "-Wl,--out-implib,$obj/bootstrap.lib"
    },
}

local exe = lm.os == "windows" and ".exe" or ""

lm:copy "copy_script" {
    input = "bootstrap/main.lua",
    output = "$bin/main.lua",
    deps = "bootstrap",
}

if not lm.notest then
    local tests = {}
    local fs = require "bee.filesystem"
    local rootdir = fs.path(lm.workdir) / ".."
    for file in fs.pairs(rootdir / "test", "r") do
        if file:equal_extension ".lua" then
            tests[#tests+1] = fs.relative(file, rootdir):lexically_normal():string()
        end
    end
    table.sort(tests)

    lm:rule "test" {
        "$bin/bootstrap"..exe, "@test/test.lua", "--touch", "$out",
        description = "Run test.",
        pool = "console",
    }
    lm:build "test" {
        rule = "test",
        deps = { "bootstrap", "copy_script" },
        input = tests,
        output = "$obj/test.stamp",
    }
end
