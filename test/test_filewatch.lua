local lt = require 'ltest'
local filewatch = require 'bee.filewatch'
local fs = require 'bee.filesystem'
local platform = require 'bee.platform'
local thread = require 'bee.thread'

local test_fw = lt.test "filewatch"

local isWindows = platform.os == 'windows'

local function supportedSymlink()
    if not isWindows then
        return true
    end
    -- see https://blogs.windows.com/windowsdeveloper/2016/12/02/symlinks-windows-10/
    local ok = pcall(fs.create_symlink, "temp.txt", "temp.link")
    fs.remove_all "temp.link"
    return ok
end

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
    fw:set_follow_symlinks(true)
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
-- test directory symlink link to parent
function test_fw:test_symlink()
    if not supportedSymlink() then
        return
    end

    local root = fs.absolute('./temp/'):lexically_normal()
    pcall(fs.remove_all, root)
    fs.create_directories(root)
    fs.create_symlink(root / 'test1', root/'test1')
    fs.create_symlink(root, root/'child')

    local fw = filewatch.create()
    fw:set_recursive(true)
    fw:set_follow_symlinks(true)
    fw:add(root:string())
    pcall(fs.remove_all, root)
end
