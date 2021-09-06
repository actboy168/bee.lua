local lm = require 'luamake'
if lm.EXE == "lua" then
    lm:import 'project/lua.lua'
else
    lm:import 'project/bootstrap.lua'
end
