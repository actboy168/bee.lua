local lm = require 'luamake'
if lm.EXE == "lua" then
    lm:import 'compile/lua.lua'
    return
end

lm:import 'compile/bootstrap.lua'
