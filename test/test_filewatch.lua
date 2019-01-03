local lu = require 'luaunit'
local fw = require 'bee.filewatch'
local fs = require 'bee.filesystem'
local platform = require 'bee.platform'
local thread = require 'bee.thread'

test_fw = {}

local function create_dir(filename)
    fs.create_directories(fs.path 'temp' / filename)
end

local function create_file(filename, content)
    os.remove(filename)
    local f = assert(io.open('temp/' .. filename, 'wb'))
    if content ~= nil then
        f:write(content)
    end
    f:close()
end

local function modify_file(filename, content)
    local f = assert(io.open('temp/' .. filename, 'ab'))
    f:write(content)
    f:close()
end

local function assertSelect(what, value)
    while true do
        local w, v = fw.select()
        if w then
            lu.assertEquals(w, what, v)
            if type(value) == 'userdata' or type(value) == 'table' then
                lu.assertEquals(fs.path(v), value)
            else
                lu.assertEquals(v, value)
            end
            break
        else
            thread.sleep(0)
        end
    end
end

local function test(f)
    local root = fs.absolute(fs.path './temp')
    pcall(fs.remove_all, root)
    fs.create_directories(root)
    local id = fw.add(root:string())
    lu.assertNumber(id)
    assertSelect('confirm', ('add `%d` `%s`'):format(id, root:string()))

    f(root)

    fw.remove(id)
    assertSelect('confirm', ('remove `%d`'):format(id))
    pcall(fs.remove_all, root)
end

function test_fw:test_1()
    test(function()
    end)
end

function test_fw:test_2()
    test(function(root)
        create_dir 'test1'
        create_file 'test1.txt'
        fs.rename(root / 'test1.txt', root / 'test2.txt')
        fs.remove(root / 'test2.txt')

        local list = {}
        local n = 10
        while true do
            local w, v = fw.select()
            if w then
                n = 10
                if type(v) == 'userdata' or type(v) == 'table' then
                    list[v:string()] = true
                else
                    list[v] = true
                end
            else
                n = n - 1
                if n < 0 then
                    break
                end
                thread.sleep(0.001)
            end
        end
        local function assertHas(path)
            lu.assertIsTrue(list[path:string()])
        end
        assertHas(root / 'test1')
        assertHas(root / 'test1.txt')
        assertHas(root / 'test2.txt')
    end)
end
