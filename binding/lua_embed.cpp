#include <bee/lua/binding.h>

static const char script[] = R"(
local sp = require 'bee.subprocess'
local fs = require 'bee.filesystem'
local platform = require 'bee.platform'

local function shell_bash(option)
    local s = {}
    for _, opt in ipairs(option) do
        s[#s+1] = sp.quotearg(opt)
    end
    option[1] = '/bin/sh'
    option[2] = '-c'
    option[3] = table.concat(s, " ")
    option[4] = nil
    return sp.spawn(option)
end

local function shell_mingw(option)
    local s = {}
    for _, opt in ipairs(option) do
        s[#s+1] = sp.quotearg(opt)
    end
    option[1] = 'sh'
    option[2] = '-c'
    option[3] = table.concat(s, " ")
    option[4] = nil
    option.searchPath = true
    return sp.spawn(option)
end

local function shell_win32(option)
    local file = fs.path(os.getenv 'COMSPEC' or 'cmd.exe')
    local iscmd = file:filename() == fs.path('cmd.exe')
    if not file:is_absolute() then
        option.searchPath = true
    end
    local args = iscmd and {file, '/d', '/s', '/c'} or {file, '-c'}
    for i, v in ipairs(args) do
        table.insert(option, i, v)
    end
    return sp.spawn(option)
end

if platform.OS == 'Windows' then
    if os.getenv 'MSYSTEM' == nil then
        sp.shell = shell_win32
    else
        sp.shell = shell_mingw
    end
else
    sp.shell = shell_bash
end

)";

BEE_LUA_API
int luaopen_bee(lua_State* L) {
    if (luaL_loadbuffer(L, script, sizeof(script) - 1, "=module 'bee'") != LUA_OK) {
        return lua_error(L);
    }
    lua_call(L, 0, 1);
    return 1;
}
