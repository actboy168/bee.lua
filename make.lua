local lm = require 'luamake'
if lm.EXE == "lua" then
    lm:import 'project/lua.lua'
    return
end

if lm.universal then
    local function IsArm64Macos()
        local f <close> = assert(io.popen("uname -v", "r"))
        if f:read "l":match "RELEASE_ARM64" then
            return true
        end
    end
    if lm.os == "macos" and IsArm64Macos() then
        lm:import 'project/macos-universal.lua'
        return
    end
    error "universal does not support"
end

lm:import 'project/bootstrap.lua'
