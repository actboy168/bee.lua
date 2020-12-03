#include <bee/lua/binding.h>

static const char script[] = R"(
local sp = require 'bee.subprocess'
local fs = require 'bee.filesystem'
local platform = require 'bee.platform'

local function quote_arg(s)
    if type(s) ~= 'string' then
        s = tostring(s)
    end
    if #s == 0 then
        return '""'
    end
    if not s:find('[ \t\"]', 1) then
        return s
    end
    if not s:find('[\"\\]', 1) then
        return '"'..s..'"'
    end
    local quote_hit = true
    local t = {}
    t[#t+1] = '"'
    for i = #s, 1, -1 do
        local c = s:sub(i,i)
        t[#t+1] = c
        if quote_hit and c == '\\' then
            t[#t+1] = '\\'
        elseif c == '"' then
            quote_hit = true
            t[#t+1] = '\\'
        else
            quote_hit = false
        end
    end
    t[#t+1] = '"'
    for i = 1, #t // 2 do
        local tmp = t[i]
        t[i] = t[#t-i+1]
        t[#t-i+1] = tmp
    end
    return table.concat(t)
end

local function shell_bash(option)
    if option.argsStyle == 'string' then
        option[3] = ('%s %s'):format(quote_arg(option[1]), option[2])
    else
        local s = {}
        for _, opt in ipairs(option) do
            s[#s+1] = quote_arg(opt)
        end
        option[3] = table.concat(s, " ")
    end
    option[1] = '/bin/sh'
    option[2] = '-c'
    option[4] = nil
    option.argsStyle = nil
    return sp.spawn(option)
end

local function shell_win(option)
    if os.getenv 'MSYSTEM' ~= nil then
        --TODO
        --return shell_bash(option)
    end
    local file = fs.path(os.getenv 'COMSPEC' or 'cmd.exe')
    local iscmd = file:filename() == fs.path('cmd.exe')
    if not file:is_absolute() then
        option.searchPath = true
    end
    if option.argsStyle == 'string' then
        local fmt = iscmd and '/d /s /c %s %s' or '-c %s %s'
        option[2] = (fmt):format(quote_arg(option[1]), option[2])
        option[1] = file
    else
        local args = iscmd and {file, '/d', '/s', '/c'} or {file, '-c'}
        for i, v in ipairs(args) do
            table.insert(option, i, v)
        end
    end
    return sp.spawn(option)
end

if platform.OS == 'Windows' then
    sp.shell = shell_win
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
