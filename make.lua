local lm = require 'luamake'

local isWindows = lm.os == 'windows'
local exe = isWindows and ".exe" or ""

lm.c = lm.compiler == 'msvc' and 'c89' or 'c11'
lm.cxx = 'c++17'
local plat = (function ()
    if isWindows then
        if lm.compiler == "gcc" then
            return "mingw"
        end
        return "msvc"
    end
    return lm.os
end)()
lm.builddir = ("build/%s"):format(plat)


lm.windows = {
    defines = "_WIN32_WINNT=0x0601",
}
if lm.mode == "debug" and lm.arch == "x86_64" and lm.compiler == "msvc" then
    lm.windows.ldflags = "/STACK:"..0x160000
end

lm.macos = {
    flags = "-Wunguarded-availability",
    sys = "macos10.12"
}
lm.linux = {
    flags = "-fPIC",
}

require 'project.common'
require 'project.bootstrap'
require 'project.lua'

lm:copy "copy_script" {
    input = "bootstrap/main.lua",
    output = "$bin/main.lua",
    deps = "bootstrap",
}

lm:build "test" {
    "$bin/bootstrap"..exe, "@test/test.lua",
    deps = { "bootstrap", "copy_script" },
    pool = "console",
    windows = {
        deps = "lua54"
    }
}

lm:default "test"
