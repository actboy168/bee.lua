local lm = require 'luamake'

lm.c = lm.plat == 'msvc' and 'c89' or 'c11'
lm.cxx = 'c++17'

require(('project.luamake.%s'):format(lm.plat))

local isWindows = lm.plat == 'mingw' or lm.plat == 'msvc'
local exe = isWindows and ".exe" or ""
local dll = isWindows and ".dll" or ".so"

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
