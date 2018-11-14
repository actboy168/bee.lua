local subprocess = require 'bee.subprocess'
local fs = require 'bee.filesystem'

local function getexe()
    local i = 0
    while arg[i] ~= nil do
        i = i - 1
    end
    return arg[i + 1]
end

local exe = fs.path(getexe())

local function wait_second()
    local f = io.popen('ping -n 1 127.1>nul', 'r')
    f:read 'a'
    f:close()
end

-- subprocess.spawn
local lua = subprocess.spawn { exe }
assert(lua ~= nil)

fs.remove(fs.path 'temp')
local lua = subprocess.spawn {
    exe,
    '-e', 'io.open("temp", "w"):close()'
}
assert(lua ~= nil)
wait_second()
assert(fs.exists(fs.path 'temp') == true)
fs.remove(fs.path 'temp')

-- wait
fs.remove(fs.path 'temp')
local lua = subprocess.spawn {
    exe,
    '-e', 'io.open("temp", "w"):close()'
}
assert(lua ~= nil)
assert(fs.exists(fs.path 'temp') ~= true)
lua:wait()
assert(fs.exists(fs.path 'temp') == true)
fs.remove(fs.path 'temp')

-- is_running
local lua = subprocess.spawn { exe }
assert(lua ~= nil)
assert(lua:is_running() == true)

-- kill
local lua = subprocess.spawn { exe }
assert(lua ~= nil)
assert(lua:is_running() == true)
assert(lua:kill() == true)
assert(lua:is_running() == false)
assert(lua:kill() == false)

-- get_id
local lua = subprocess.spawn { exe }
assert(lua ~= nil)
local id = lua:get_id()
local f = io.popen(('tasklist /FI "PID eq %d"'):format(id), 'r')
local buf = f:read 'a'
f:close()
assert(buf:find(exe:filename():string(), 1, true) ~= nil)

--
local lua, stdin, stdout, stderr = subprocess.spawn {
    exe,
    stdin = true,
    stdout = true,
    stderr = true,
}
assert(lua ~= nil)
assert(stdin ~= nil)
assert(stdout ~= nil)
assert(stderr ~= nil)
