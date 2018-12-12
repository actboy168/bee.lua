local subprocess = require 'bee.subprocess'
local fs = require 'bee.filesystem'
local thread = require 'bee.thread'

local exe = fs.exe_path()

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
lua:wait()
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
local lua = subprocess.spawn {
    exe,
    '-e', 'io.write("ok")',
    stdout = true
}
lua:wait()
assert("ok" == lua.stdout:read 'a')

-- subprocess.peek
local lua = subprocess.spawn {
    exe,
    '-e', 'io.write("ok")',
    stdout = true
}
lua:wait()
assert(subprocess.peek(lua.stdout) == 2)

-- subprocess.filemode
local lua = subprocess.spawn {
    exe,
    '-e', 'io.write(io.read "a")',
    stdin = true,
    stdout = true,
}
subprocess.filemode(lua.stdin, 'b')
subprocess.filemode(lua.stdout, 'b')
lua.stdin:write '\r\n'
lua.stdin:close()
lua:wait()
assert(subprocess.peek(lua.stdout) == 2)
