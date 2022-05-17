local lt = require 'ltest'
local fw = require 'bee.filewatch'
local fs = require 'bee.filesystem'
local thread = require 'bee.thread'
local shell = require 'shell'

local test_fw = lt.test "filewatch"

local function create_file(filename, content)
    fs.remove(filename)
    local f <close> = assert(io.open(filename:string(), 'wb'))
    if content ~= nil then
        f:write(content)
    end
end

local function assertSelect(what, value)
    while true do
        local w, v = fw.select()
        if w then
            lt.assertEquals(w, what)
            if type(value) == 'userdata' or type(value) == 'table' then
                lt.assertEquals(fs.path(v), value)
            else
                lt.assertEquals(v, value)
            end
            break
        else
            thread.sleep(0)
        end
    end
end

local function test(f)
    local root = fs.path './temp/'

    pcall(fs.remove_all, root)
    fs.create_directories(root)
    local id = fw.add(root:string())
    lt.assertIsNumber(id)
    assertSelect('task_add', ('(%d)%s'):format(id, root:string()))

    f(root)

    fw.remove(id)
    assertSelect('task_remove', ('%d'):format(id))
    pcall(fs.remove_all, root)
end

function test_fw:test_1()
    test(function()
    end)
end

function test_fw:test_2()
    test(function(root)
        fs.create_directories(root / 'test1')
        create_file(root / 'test1.txt')
        fs.rename(root / 'test1.txt', root / 'test2.txt')
        fs.remove(root / 'test2.txt')

        local list = {}
        local n = 100
        while true do
            local w, v = fw.select()
            if w then
                n = 100
                if type(v) == 'userdata' or type(v) == 'table' then
                    list[#list+1] = v
                else
                    list[#list+1] = fs.path(v)
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
            for _, v in ipairs(list) do
                if v == path then
                    return
                end
            end
            lt.assertEquals(path, nil)
        end
        assertHas(root / 'test1')
        assertHas(root / 'test1.txt')
        assertHas(root / 'test2.txt')
    end)
end
