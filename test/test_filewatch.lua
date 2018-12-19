local lu = require 'luaunit'
local fw = require 'bee.filewatch'
local fs = require 'bee.filesystem'
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
            if type(value) == 'userdata' then
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
        assertSelect('create', root / 'test1')
        
        create_file 'test1.txt'
        assertSelect('create', root / 'test1.txt')

        fs.rename(root / 'test1.txt', root / 'test2.txt')
        assertSelect('rename', root / 'test1.txt')
        assertSelect('rename', root / 'test2.txt')

        modify_file('test2.txt', 'ok')
        assertSelect('modify', root / 'test2.txt')

        fs.remove(root / 'test2.txt')
        assertSelect('delete', root / 'test2.txt')
    end)
end
