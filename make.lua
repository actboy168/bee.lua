local lm = require 'luamake'
if lm.EXE == "lua" then
    lm:import 'project/lua.lua'
    return
end

if lm.universal then
    if lm.os == "macos" then
        lm:import 'project/macos-universal.lua'
        return
    end
    error "universal does not support"
end

lm:import 'project/bootstrap.lua'
