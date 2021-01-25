local level
local buffer
local visited
local putValue

local escape_char = {
    [ "\\" .. string.byte "\a" ] = "\\".."a",
    [ "\\" .. string.byte "\b" ] = "\\".."b",
    [ "\\" .. string.byte "\f" ] = "\\".."f",
    [ "\\" .. string.byte "\n" ] = "\\".."n",
    [ "\\" .. string.byte "\r" ] = "\\".."r",
    [ "\\" .. string.byte "\t" ] = "\\".."t",
    [ "\\" .. string.byte "\v" ] = "\\".."v",
    [ "\\" .. string.byte "\\" ] = "\\".."\\",
    [ "\\" .. string.byte "\"" ] = "\\".."\"",
}

local function quoted(s)
    return ("%q"):format(s):sub(2,-2):gsub("\\[1-9][0-9]?", escape_char):gsub("\\\n", "\\n")
end

local function isIdentifier(str)
    return type(str) == 'string' and str:match "^[_%a][_%a%d]*$"
end

local typeOrders = {
    ['number']   = 1, ['boolean']  = 2, ['string'] = 3, ['table'] = 4,
    ['function'] = 5, ['userdata'] = 6, ['thread'] = 7, ['nil']   = 8,
}

local function sortKeys(a, b)
    local ta, tb = type(a), type(b)
    if ta == tb then
        if ta == 'string' or ta == 'number' then
            return a < b
        end
        return false
    end
    return typeOrders[ta] < typeOrders[tb]
end

local function getLength(t)
    local length = 1
    while rawget(t, length) ~= nil do
        length = length + 1
    end
    return length - 1
end

local function puts(v)
    buffer[#buffer+1] = v
end

local function down(f)
    level = level + 1
    f()
    level = level - 1
end

local function tabify()
    puts '\n'
    puts(string.rep('  ', level))
end

local function alreadyVisited(t)
    local v = visited[t]
    if not v then
        visited[t] = true
    end
    return v
end

local function putKey(k)
    if isIdentifier(k) then return puts(k) end
    puts "["
    putValue(k)
    puts "]"
end

local function putTable(t)
    if alreadyVisited(t) then
        puts '<table>'
        return
    end
    local keys = {}
    local length = getLength(t)
    for k in next, t do
        if math.type(k) ~= 'integer' or k < 1 or k > length then
            keys[#keys+1] = k
        end
    end
    table.sort(keys, sortKeys)
    local mt = getmetatable(t)
    puts '{'
    down(function()
        local first = true
        for i = 1, length do
            if not first then puts ',' end
            puts ' '
            putValue(rawget(t, i))
            first = false
        end
        for _, k in ipairs(keys) do
            if not first then puts ',' end
            tabify()
            putKey(k)
            puts ' = '
            putValue(rawget(t, k))
            first = false
        end
        if type(mt) == 'table' then
            if not first then puts ',' end
            tabify()
            puts '<metatable> = '
            putValue(mt)
        end
    end)
    if #keys > 0 or type(mt) == 'table' then
        tabify()
    elseif length > 0 then
        puts ' '
    end
    puts '}'
end

local function putTostring(v)
    puts '<'
    puts(tostring(v))
    puts '>'
end

local function putUserdata(u)
    local mt = debug.getmetatable(u)
    if mt and mt.__tostring then
        puts '<userdata:'
        puts(tostring(u))
        puts '>'
    else
        putTostring(u)
    end
end

local function putThread(t)
    putTostring(t)
end

local function putFunction(f)
    local info = debug.getinfo(f, "S")
    local type = info.source:sub(1,1)
    if type == "@" then
        puts '<function:'
        puts(info.source:sub(2))
        puts '>'
    elseif type == "=" then
        putTostring(f)
    else
        puts '<function:'
        if #info.source > 64 then
            puts(info.source:sub(1,64))
            puts '...'
        else
            puts(info.source)
        end
        puts '>'
    end
end

function putValue(v)
    local tv = type(v)
    if tv == 'string' then
        puts(quoted(v))
    elseif tv == 'number' or tv == 'boolean' or tv == 'nil' then
        puts(tostring(v))
    elseif tv == 'table' then
        putTable(v)
    elseif tv == 'userdata' then
        putUserdata(v)
    elseif tv == 'function' then
        putFunction(v)
    else
        assert(tv == 'thread')
        putThread(v)
    end
end

return function (root)
    level   = 0
    buffer  = {}
    visited = {}
    putValue(root)
    return table.concat(buffer)
end
