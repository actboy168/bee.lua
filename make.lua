local lm = require "luamake"

--lm.lua = "55"

require "compile.common"

if lm.EXE == "lua" then
    require "compile.lua"
else
    require "compile.bootstrap"
end
