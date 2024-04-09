local subprocess = require "bee.subprocess"
local platform = require "bee.platform"

local luaexe = platform.os == "windows"
    and "./build/bin/bootstrap.exe"
    or "./build/bin/bootstrap"

local process = assert(subprocess.spawn {
    luaexe, "test/test.lua", arg,
    stdout = io.stdout,
    stderr = "stdout",
})

local code = process:wait()
if code ~= 0 then
    os.exit(code, true)
end
