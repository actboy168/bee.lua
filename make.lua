local lm = require 'luamake'
if lm.EXE == "lua" then
    lm:import 'project/lua.lua'
    return
end

lm:import 'project/bootstrap.lua'
