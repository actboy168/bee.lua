local subprocess = require "bee.subprocess"
local thread = require "bee.thread"
local EXE = (require "bee.platform".OS == "Windows") and ".exe" or ""

local process = assert(subprocess.spawn {
    "build/bin/bootstrap"..EXE, "test/test.lua", arg,
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
