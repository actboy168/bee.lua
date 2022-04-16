local lm = require 'luamake'

lm.compile_commands = "$builddir"

if lm.EXE == "lua" then
    lm:import 'compile/lua.lua'
    return
end

lm:import 'compile/bootstrap.lua'
