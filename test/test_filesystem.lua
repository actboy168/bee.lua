local lu = require 'luaunit'

local fs = require 'bee.filesystem'
local platform = require 'bee.platform'
local thread = require 'bee.thread'

local shell = {}
function shell:add_readonly(filename)
    os.execute(('attrib +r %q'):format(filename))
end
function shell:del_readonly(filename)
    os.execute(('attrib -r %q'):format(filename))
end

test_fs = {}

function test_fs:test_path()
    lu.assertUserdata(fs.path(''))
end

function test_fs:test_string()
    lu.assertEquals(fs.path('a\\b'):string(), 'a\\b')
    lu.assertEquals(fs.path('a/b'):string(), 'a/b')
end

function test_fs:test_filename()
    local function get_filename(path)
        return fs.path(path):filename():string()
    end
    lu.assertEquals(get_filename('a/b'), 'b')
    lu.assertEquals(get_filename('a/b/'), '')
    lu.assertEquals(get_filename('a\\b'), 'b')
    lu.assertEquals(get_filename('a\\b\\'), '')
end

function test_fs:test_parent_path()
    local function get_parent_path(path)
        return fs.path(path):parent_path():string()
    end
    lu.assertEquals(get_parent_path('a/b/c'), 'a/b')
    lu.assertEquals(get_parent_path('a/b/'), 'a/b')
    lu.assertEquals(get_parent_path('a\\b\\c'), 'a\\b')
    lu.assertEquals(get_parent_path('a\\b\\'), 'a\\b')
end

function test_fs:test_stem()
    local function get_stem(path)
        return fs.path(path):stem():string()
    end
    lu.assertEquals(get_stem('a/b/c.ext'), 'c')
    lu.assertEquals(get_stem('a/b/c'), 'c')
    lu.assertEquals(get_stem('a/b/.ext'), '.ext')
    lu.assertEquals(get_stem('a\\b\\c.ext'), 'c')
    lu.assertEquals(get_stem('a\\b\\c'), 'c')
    lu.assertEquals(get_stem('a\\b\\.ext'), '.ext')
end

function test_fs:test_extension()
    local function get_extension(path)
        return fs.path(path):extension():string()
    end
    lu.assertEquals(get_extension('a/b/c.ext'), '.ext')
    lu.assertEquals(get_extension('a/b/c'), '')
    lu.assertEquals(get_extension('a/b/.ext'), '')
    lu.assertEquals(get_extension('a\\b\\c.ext'), '.ext')
    lu.assertEquals(get_extension('a\\b\\c'), '')
    lu.assertEquals(get_extension('a\\b\\.ext'), '')
    
    lu.assertEquals(get_extension('a/b/c.'), '.')
    lu.assertEquals(get_extension('a/b/c..'), '.')
    lu.assertEquals(get_extension('a/b/c..lua'), '.lua')
end

function test_fs:test_absolute_relative()
    local function assertIsAbsolute(path)
        lu.assertIsTrue(fs.path(path):is_absolute(), path)
        lu.assertIsFalse(fs.path(path):is_relative(), path)
    end
    local function assertIsRelative(path)
        lu.assertIsFalse(fs.path(path):is_absolute(), path)
        lu.assertIsTrue(fs.path(path):is_relative(), path)
    end
    assertIsAbsolute('c:/a/b')
    assertIsAbsolute('//a/b')
    assertIsRelative('./a/b')
    assertIsRelative('a/b')
    assertIsRelative('../a/b')
    if platform.os() == 'Windows' then
        assertIsRelative('/a/b')
    else
        assertIsAbsolute('/a/b')
    end
end

function test_fs:test_remove_filename()
    local function remove_filename(path)
        return fs.path(path):remove_filename():string()
    end
    lu.assertEquals(remove_filename('a/b/c'), 'a/b/')
    lu.assertEquals(remove_filename('a/b/'), 'a/b/')
    lu.assertEquals(remove_filename('a\\b\\c'), 'a\\b\\')
    lu.assertEquals(remove_filename('a\\b\\'), 'a\\b\\')
end

function test_fs:test_replace_extension()
    local function replace_extension(path, ext)
        return fs.path(path):replace_extension(ext):string()
    end
    lu.assertEquals(replace_extension('a/b/c.ext', '.lua'), 'a/b/c.lua')
    lu.assertEquals(replace_extension('a/b/c', '.lua'), 'a/b/c.lua')
    lu.assertEquals(replace_extension('a/b/.ext', '.lua'), 'a/b/.ext.lua')
    lu.assertEquals(replace_extension('a\\b\\c.ext', '.lua'), 'a\\b\\c.lua')
    lu.assertEquals(replace_extension('a\\b\\c', '.lua'), 'a\\b\\c.lua')
    lu.assertEquals(replace_extension('a\\b\\.ext', '.lua'), 'a\\b\\.ext.lua')

    lu.assertEquals(replace_extension('a/b/c.ext', 'lua'), 'a/b/c.lua')
    lu.assertEquals(replace_extension('a/b/c.ext', '..lua'), 'a/b/c..lua')
end

local ALLOW_WRITE = 0x92

function test_fs:test_permissions()
    local filename = 'temp.txt'
    io.open(filename, 'w'):close()

    lu.assertEquals(fs.path(filename):permissions() & ALLOW_WRITE, ALLOW_WRITE)
    shell:add_readonly(filename)
    lu.assertEquals(fs.path(filename):permissions() & ALLOW_WRITE, 0)
    shell:del_readonly(filename)

    os.remove(filename)
end

function test_fs:test_add_remove_permissions()
    local filename = 'temp.txt'
    io.open(filename, 'w'):close()
    
    lu.assertEquals(fs.path(filename):permissions() & ALLOW_WRITE, ALLOW_WRITE)
    fs.path(filename):remove_permissions(ALLOW_WRITE)
    lu.assertEquals(fs.path(filename):permissions() & ALLOW_WRITE, 0)
    fs.path(filename):add_permissions(ALLOW_WRITE)
    lu.assertEquals(fs.path(filename):permissions() & ALLOW_WRITE, ALLOW_WRITE)

    os.remove(filename)
end

function test_fs:test_div()
    local function div(A, B)
        return (fs.path(A) / B):string()
    end
    lu.assertEquals(div('a', 'b'), 'a\\b')
    lu.assertEquals(div('a\\b', 'c'), 'a\\b\\c')
    lu.assertEquals(div('a/b', 'c'), 'a/b\\c')
    lu.assertEquals(div('a/', 'b'), 'a/b')
    lu.assertEquals(div('a\\', 'b'), 'a\\b')
    lu.assertEquals(div('a', '/b'), '/b')
    lu.assertEquals(div('a/', '\\b'), '\\b')
    lu.assertEquals(div('c:/a', '/b'), 'c:/b')
    lu.assertEquals(div('c:/a/', '\\b'), 'c:\\b')
    lu.assertEquals(div('c:/a', 'd:/b'), 'd:/b')
    lu.assertEquals(div('c:/a/', 'd:\\b'), 'd:\\b')
end

function test_fs:test_absolute()
    local function eq_absolute1(path)
        return lu.assertEquals(fs.absolute(fs.path(path)):string(), (fs.current_path() / path):string())
    end
    local function eq_absolute2(path1, path2)
        return lu.assertEquals(fs.absolute(fs.path(path1)):string(), (fs.current_path() / path2):string())
    end
    eq_absolute1('a\\')
    eq_absolute1('a\\b')
    eq_absolute1('a\\b\\')
    eq_absolute2('a/b', 'a\\b')
    eq_absolute2('./b', 'b')
    eq_absolute2('a/../b', 'b')
    eq_absolute2('a/b/../', 'a\\')
end

function test_fs:test_eq()
    local function eq(A, B)
        return lu.assertEquals(fs.path(A), fs.path(B))
    end
    eq('a/b', 'a/b')
    eq('a/b', 'a\\b')
    eq('a/B', 'a/b')
    eq('a/./b', 'a/b')
    eq('a/b/../c', 'a/c')
    eq('a/b/../c', 'a/d/../c')
end

--[==[

-- list_directory
--TODO



-- fs.exists
local path = fs.path('temp.txt')
assert(fs.exists(path) == false)
io.open(path:string(), 'w'):close()
assert(fs.exists(path) == true)
os.remove(path:string())

-- is_directory
local path = fs.path('temp.txt')
io.open(path:string(), 'w'):close()
assert(fs.is_directory(path) == false)
os.remove(path:string())

local dir = fs.path('..')
assert(fs.is_directory(dir) == true)

-- create_directory
local dir = fs.path('tempdir')
fs.create_directory(dir)
assert(fs.is_directory(dir) == true)
while fs.exists(dir) do
    thread.sleep(0.1)
    os.execute('rd /Q ' .. dir:string())
end

-- create_directories
local dir = fs.path('tempdir')
local dirs = dir / 'a' / 'b'
fs.create_directories(dirs)
assert(fs.is_directory(dir) == true)
assert(fs.is_directory(dirs) == true)
while fs.exists(dir) do
    thread.sleep(0.1)
    os.execute('rd /S /Q ' .. dir:string())
end

-- rename
local path1 = fs.path('temp')
local path2 = fs.path('temp_renamed')
local content = tostring(os.time())
local f = io.open(path1:string(), 'wb')
f:write(content)
f:close()
fs.rename(path1, path2)
assert(fs.exists(path1) == false)
assert(fs.exists(path2) == true)
local f = io.open(path2:string(), 'rb')
assert(content == f:read 'a')
f:close()
os.remove(path2:string())

-- remove
local path = fs.path('temp')
io.open(path:string(), 'wb'):close()
assert(fs.exists(path) == true)
fs.remove(path)
assert(fs.exists(path) == false)

local dir = fs.path('tempdir')
fs.create_directory(dir)
assert(fs.exists(dir) == true)
fs.remove(dir)
assert(fs.exists(dir) == false)

-- remove_all
local dir = fs.path('tempdir')
local dirs = dir / 'a' / 'b'
fs.create_directories(dirs)
assert(fs.is_directory(dir) == true)
assert(fs.is_directory(dirs) == true)
fs.remove_all(dir)
assert(fs.is_directory(dir) == false)

-- current_path
local current = io.popen('echo %cd%', 'r'):read 'l'
assert(fs.current_path():string() == current)

-- copy_file
local path1 = fs.path('temp1')
local path2 = fs.path('temp2')
local content = tostring(os.time())

local f = io.open(path1:string(), 'wb')
f:write(content)
f:close()
fs.copy_file(path1, path2)
assert(fs.exists(path1) == true)
assert(fs.exists(path2) == true)
local f = io.open(path1:string(), 'rb')
assert(f:read 'a' == content)
f:close()
local f = io.open(path2:string(), 'rb')
assert(f:read 'a' == content)
f:close()
fs.remove(path1)
fs.remove(path2)
assert(fs.exists(path1) == false)
assert(fs.exists(path2) == false)

local f = io.open(path1:string(), 'wb')
f:write(content)
f:close()
fs.copy_file(path1, path2)
io.open(path2:string(), 'wb'):close()
assert(fs.exists(path1) == true)
assert(fs.exists(path2) == true)
local suc, err  = pcall(fs.copy_file, path1, path2)
assert(suc == false, err)
local suc, err = pcall(fs.copy_file, path1, path2, true)
assert(suc == true, err)
local f = io.open(path1:string(), 'rb')
assert(f:read 'a' == content)
f:close()
local f = io.open(path2:string(), 'rb')
assert(f:read 'a' == content)
f:close()
fs.remove(path1)
fs.remove(path2)
assert(fs.exists(path1) == false)
assert(fs.exists(path2) == false)

-- absolute
local path = fs.path('D:/A\\B\\../C')
assert(fs.absolute(path):string() == 'D:\\A\\C')

local path = fs.path('temp')
assert((fs.current_path() / path):string() == fs.absolute(path):string())

local path = fs.path('temp')
local base = fs.path('D:\\')
assert((base / path):string() == fs.absolute(path, base):string())

-- relative
local path = fs.path('D:\\A\\C')
local base = fs.path('D:\\')
assert(fs.relative(path, base):string() == 'A\\C')

local path = fs.path('D:/A\\B\\../C')
local base = fs.path('D:\\')
assert(fs.relative(path, base):string() == 'A\\C')

local path = fs.path('C:\\A')
local base = fs.path('D:\\')
assert(fs.relative(path, base):string() == '')

-- last_write_time
local path = fs.path('temp')
local f = io.open(path:string(), 'wb')
local time1 = fs.last_write_time(path)
thread.sleep(0.1)
f:write('a')
f:close()
local time2 = fs.last_write_time(path)
fs.remove(path)
assert(time2 > time1)

-- exe_path
local function getexe()
    local i = 0
    while arg[i] ~= nil do
        i = i - 1
    end
    return fs.absolute(fs.path(arg[i + 1]))
end
assert(fs.exe_path() == getexe())
]==]
