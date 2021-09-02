local lm = require 'luamake'

local isWindows = lm.os == 'windows'
local exe = isWindows and ".exe" or ""
local dll = isWindows and ".dll" or ".so"

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

require(('project.%s'):format(plat))
require 'project.common'

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
