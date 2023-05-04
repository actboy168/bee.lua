local lm = require "luamake"

if lm.compiler == "emcc" then
    lm:import 'compile/emcc.lua'
    return
end

lm.compile_commands = "$builddir"

if lm.EXE == "lua" then
    lm:import "compile/lua.lua"
    return
end

lm:import "compile/bootstrap.lua"
