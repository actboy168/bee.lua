local lm = require "luamake"

require "compile.common"

if lm.EXE == "lua" then
    require "compile.lua"
else
    require "compile.bootstrap"
end
