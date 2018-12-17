local fs = require 'bee.filesystem'
local cwd = fs.exe_path():parent_path():parent_path()

local function readfile(filename)
    local f = assert(io.open(filename, "rb"))
    local str = f:read "a"
    f:close()
    return str
end

local function writefile(filename, str)
    local f = assert(io.open(filename, "wb"))
    f:write(str)
    f:close()
end

local function compile(name, filename)
    local str = readfile(filename)
    local t = {}
    t[#t+1] = ('inline const char %s[] = {'):format(name)
    for n = 1, #str + 1 do
        if n % 16 == 1 then
            t[#t+1] = '\r\n    '
        end
        t[#t+1] = ('0x%02x, '):format(str:byte(n) or 0)
    end
    t[#t+1] = '\r\n};\r\n'
    return table.concat(t)
end

local function remove(source, name)
    local pattern = ('inline const char %s'):format(name:gsub('[%^%$%(%)%%%.%[%]%*%+%-%?]', '%%%0'))
    return source:gsub(pattern .. '%[%] = {[0-9,%l%s]*};\r\n', '')
end

local function update(sourcefile, name, value)
    if not fs.exists(fs.path(sourcefile)) then
        writefile(sourcefile, [[
//
// DO NOT EDIT
//
]])
    end
    local source = readfile(sourcefile)
    writefile(sourcefile, remove(source, name) .. value)
end

local function compile_update(name, filename, output)
    update(output, name, compile(name, filename))
end

local function scan(output, filename)
    local source = readfile(filename)
    for name, filename in source:gmatch 'nonstd::embed[%s]*%([%s]*([%w_]+)[%s]*,[%s]*"([^"]*)"[%s]*%)' do
        compile_update(name, fs.absolute(fs.path(filename), cwd):string(), output)
    end
end

local output, filename = ...
if not filename then
    fs.remove(fs.path(output))
    return
end
scan(output, filename)
