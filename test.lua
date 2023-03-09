local subprocess = require "bee.subprocess"
local thread = require "bee.thread"

local luaexe = (function()
    local i = 0
    while arg[i] ~= nil do
        i = i - 1
    end
    return arg[i + 1]
end)()

local process = assert(subprocess.spawn {
    luaexe, "test/test.lua", arg,
    stdout = true,
    stderr = "stdout",
})

while true do
    local n = subprocess.peek(process.stdout)
    if n == nil then
        break
    end
    if n == 0 then
        thread.sleep(0.1)
    else
        local data = process.stdout:read(n)
        io.write(data)
        io.flush()
    end
end

process.stdout:close()

local code = process:wait()
if code ~= 0 then
    os.exit(code, true)
end
