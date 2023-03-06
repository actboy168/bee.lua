local lt = require 'ltest'
local filewatch = require 'bee.filewatch'
local fs = require 'bee.filesystem'
local thread = require 'bee.thread'

local test_fw = lt.test "filewatch"

local function create_file(filename, content)
    fs.remove(filename)
    local f <close> = assert(io.open(filename:string(), 'wb'))
    if content ~= nil then
        f:write(content)
    end
end

local function test(f)
    local root = fs.absolute('./temp/'):lexically_normal()
    pcall(fs.remove_all, root)
    fs.create_directories(root)
    local fw = filewatch.create()
    fw:set_recursive(true)
    fw:set_follow_symlinks(false)
    fw:set_filter(function ()
        return true
    end)
    fw:add(root:string())
    f(fw, root)
    pcall(fs.remove_all, root)
end

function test_fw:test_1()
    test(function()
    end)
end

function test_fw:test_2()
    test(function(fw, root)
        fs.create_directories(root / 'test1')
        create_file(root / 'test1.txt')
        fs.rename(root / 'test1.txt', root / 'test2.txt')
        fs.remove(root / 'test2.txt')

        local list = {}
        local n = 100
        while true do
            local w, v = fw:select()
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

-- test unexist symlink link to self
function test_fw:test_link_self_symlink()
    test(function(fw, root)
        local test_root = root / 'test_symlink'
        local err = fs.create_directories(test_root)
        lt.assertEquals(err, true)

        pcall(fs.remove_all, test_root/'*')

        fs.create_symlink(test_root / 'test1', test_root/'test1')

        local fw = filewatch.create()
        fw:set_recursive(true)
        fw:set_follow_symlinks(true)
        fw:add(test_root:string())

        pcall(fs.remove_all, root)
    end)
end

-- test directory symlink link to parent
function test_fw:test_directory_symlink_link_to_parent()
    test(function(fw, root)
        local test_root = root / 'test_symlink'
        local err = fs.create_directories(test_root)
        lt.assertEquals(err, true)

        pcall(fs.remove_all, test_root/'*')

        fs.create_symlink(test_root, test_root/'child')

        local fw = filewatch.create()
        fw:set_recursive(true)
        fw:set_follow_symlinks(true)
        fw:add(test_root:string())

        pcall(fs.remove_all, root)
    end)
end
