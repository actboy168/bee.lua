local subprocess = require 'bee.subprocess'
local fs = require 'bee.filesystem'
local exe = fs.path(arg[-3])

local function wait_second()
    local f = io.popen('ping -n 1 127.1>nul', 'r')
    f:read 'a'
    f:close()
end

-- spawn
local process = subprocess.spawn { exe:string() }
assert(process ~= nil)
