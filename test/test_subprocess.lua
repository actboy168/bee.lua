local subprocess = require 'bee.subprocess'
local fs = require 'bee.filesystem'

local exe = fs.exe_path()

local function wait_second()
    local f = io.popen('ping -n 1 127.1>nul', 'r')
    f:read 'a'
    f:close()
end

-- subprocess.spawn
local lua = subprocess.spawn {
    exe,
    '-e', ' '
}
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
assert(fs.exists(fs.path 'temp') ~= true)
local lua = subprocess.spawn {
    exe,
    '-e', 'io.open("temp", "w"):close()'
}
assert(lua ~= nil)
lua:wait()
assert(fs.exists(fs.path 'temp') == true)
fs.remove(fs.path 'temp')

-- is_running
local lua = subprocess.spawn {
    exe,
    '-e', ' '
}
assert(lua ~= nil)
assert(lua:is_running() == true)

-- kill
local lua = subprocess.spawn {
    exe,
    '-e', ' '
}
assert(lua ~= nil)
assert(lua:is_running() == true)
assert(lua:kill() == true)
assert(lua:is_running() == false)
assert(lua:kill() == false)

-- get_id
local lua = subprocess.spawn {
    exe,
    console = 'new',
    hideWindow = true
}
assert(lua ~= nil)
local id = lua:get_id()
local f = io.popen(('tasklist /FI "PID eq %d"'):format(id), 'r')
local buf = f:read 'a'
f:close()
lua:kill()
assert(buf:find(exe:filename():string(), 1, true) ~= nil)

-- resume TODO

-- native_handle TODO

-- stdout
local lua, stdout = subprocess.spawn {
    exe,
    '-e', 'io.write("ok")',
    stdout = true
}
lua:wait()
--assert(stdout:read 'a') TODO

-- subprocess.peek
local lua, stdout = subprocess.spawn {
    exe,
    '-e', 'io.write("ok")',
    stdout = true
}
lua:wait()
assert(subprocess.peek(stdout) == 2)

-- TODO
--[[
-- subprocess.filemode
local lua, stdin, stdout = subprocess.spawn {
    exe,
    '-e', 'io.write(io.read "a")',
    stdin = true,
    stdout = true,
}
stdin:setvbuf 'no'
stdout:setvbuf 'no'
stdin:write '\r\n'
lua:wait()
assert(subprocess.peek(stdout) == 1)

local lua, stdin, stdout = subprocess.spawn {
    exe,
    '-e', 'io.write(io.read "a")',
    stdin = true,
    stdout = true,
}
subprocess.filemode(stdin, 'b')
subprocess.filemode(stdout, 'b')
stdin:setvbuf 'no'
stdout:setvbuf 'no'
stdin:write '\r\n'
lua:wait()
assert(subprocess.peek(stdout) == 2)
]]--
