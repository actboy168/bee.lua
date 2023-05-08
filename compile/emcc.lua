local lm = require "luamake"

lm.rootdir = ".."
lm.c = "c11"
lm.cxx = "c++17"
lm.rtti = "off"
lm.flags = {
    "-pthread",
}

lm:lua_source "source_bee" {
    sources = "3rd/lua-seri/lua-seri.c",
}

lm:source_set "source_bee" {
    sources = "3rd/fmt/format.cc",
}

lm:lua_src "source_bee" {
    includes = ".",
    sources = {
        "bee/platform/version.cpp",
        "bee/thread/simplethread_posix.cpp",
        "bee/thread/setname.cpp",
        "bee/thread/spinlock.cpp",
        "bee/utility/path_helper.cpp",
        "bee/error.cpp",
    }
}

lm:lua_src "source_bee" {
    includes = ".",
    sources = {
        "binding/lua_platform.cpp",
        "binding/lua_serialization.cpp",
        "binding/lua_filesystem.cpp",
        "binding/lua_thread.cpp",
        "binding/lua_time.cpp",
        "bootstrap/bootstrap_init.cpp",
    }
}

lm:exe "lua" {
    deps = "source_bee",
    defines = {
        "LUA_USE_DLOPEN",
        "MAKE_LIB",
    },
    ldflags = {
        "-lnodefs.js",
        "-lnoderawfs.js",
        "-sENVIRONMENT=node",
        "-pthread",
    },
    sources = {
        "3rd/lua/onelua.c",
        "3rd/lua/lua.c",
    },
}

if not lm.notest then
    local tests = {}
    local fs = require "bee.filesystem"
    local rootdir = fs.path(lm.workdir) / ".."
    for file in fs.pairs(rootdir / "test", "r") do
        if file:equal_extension ".lua" then
            tests[#tests + 1] = fs.relative(file, rootdir):lexically_normal():string()
        end
    end
    table.sort(tests)

    lm:rule "test" {
        "node", "$bin/lua.js", "@test/test.lua", "--touch", "$out",
        description = "Run test.",
        pool = "console",
    }
    lm:build "test" {
        rule = "test",
        deps = "lua",
        input = tests,
        output = "$obj/test.stamp",
    }
end
