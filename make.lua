local lm = require "luamake"

if lm.EXE == "lua" then
    require "compile.common"
    require "compile.lua"
else
    --lm.luaversion = "lua55"
    require "compile.common"
    require "compile.bootstrap"
end
