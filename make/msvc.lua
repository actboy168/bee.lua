local helper = require 'msvc_helper'
local ffi = require 'ffi'
ffi.cdef[[
    int SetEnvironmentVariableA(const char* name, const char* value);
]]

local function addenv(name, newvalue)
    local oldvalue = os.getenv(name)
    if oldvalue then
        newvalue = newvalue .. ';' .. oldvalue
    end
    ffi.C.SetEnvironmentVariableA(name, newvalue)
end

for k, v in pairs(helper.get_env(helper.get_path())) do
    addenv(k, v)
end

require 'bee'
local sp = require 'bee.subprocess'
local process = assert(sp.shell {
    table.pack(...),
    stderr = io.stderr,
    stdout = io.stdout,
})
os.exit(process:wait(), true)
