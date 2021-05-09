local lm = require 'luamake'

local isWindows = lm.os == 'windows'
local exe = isWindows and ".exe" or ""
local dll = isWindows and ".dll" or ".so"

lm.c = lm.compiler == 'msvc' and 'c89' or 'c11'
lm.cxx = 'c++17'
lm.builddir = ("build/%s"):format((function ()
    if isWindows then
        if lm.compiler == "gcc" then
            return "mingw"
        end
        return "msvc"
    end
    return lm.os
end)())

require(('project.luamake.%s'):format(lm.os))

lm:copy "copy_script" {
    input = "bootstrap/main.lua",
    output = "$bin/main.lua",
    deps = "bootstrap",
}

lm:build "test" {
    "$bin/bootstrap"..exe, "@test/test.lua",
    deps = { "bootstrap", "copy_script", "bee" },
    pool = "console"
}
