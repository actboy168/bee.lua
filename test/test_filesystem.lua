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

function shell:pwd()
    local command
    if platform:os() == "Windows" then
        command = 'echo %cd%'
    else
        command = 'pwd'
    end
    return (io.popen(command):read 'a'):gsub('[\n\r]*$', '')
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


function test_fs:test_relative()
    local function relative(a, b, c)
        return lu.assertEquals(fs.relative(fs.path(a), fs.path(b)):string(), c)
    end
    relative('c:/a/b/c', 'c:/a/b', 'c')
    relative('c:/a/b/c', 'c:/a',  'b\\c')
    relative('c:\\a\\b\\c', 'c:\\a',  'b\\c')
    relative('c:/a/b', 'c:/a/b/c',  '..')
    relative('c:/a/d/e', 'c:/a/b/c',  '..\\..\\d\\e')
    relative('d:/a/b/c', 'c:/a',  '')
    relative('a', 'c:/a/b/c',  '')
    relative('c:/a/b', 'a/b/c',  '')
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

function test_fs:test_exists()
    local function is_exists(path, b)
        lu.assertEquals(fs.exists(fs.path(path)), b, path)
    end
    local filename = 'temp.txt'
    os.remove(filename)
    is_exists(filename, false)
    io.open(filename, 'w'):close()
    is_exists(filename, true)
    os.remove(filename)
    is_exists(filename, false)
end

function test_fs:test_remove()
    local function remove_ok(path, b)
        lu.assertEquals(fs.exists(fs.path(path)), b, path)
        lu.assertEquals(fs.remove(fs.path(path)), b, path)
        lu.assertIsFalse(fs.exists(fs.path(path)), path)
    end
    local function remove_failed(path)
        lu.assertIsTrue(fs.exists(fs.path(path)), path)
        lu.assertError(fs.remove, fs.path(path))
        lu.assertIsTrue(fs.exists(fs.path(path)), path)
    end

    local filename = 'temp.txt'
    os.remove(filename)
    io.open(filename, 'w'):close()
    remove_ok(filename, true)
    remove_ok(filename, false)

    local filename = 'temp'
    fs.create_directories(fs.path(filename))
    remove_ok(filename, true)
    remove_ok(filename, false)

    local filename = 'temp/temp'
    fs.create_directories(fs.path(filename))
    remove_ok(filename, true)
    remove_ok(filename, false)
    
    local filename = 'temp'
    fs.create_directories(fs.path(filename))
    io.open('temp/temp.txt', 'w'):close()
    remove_failed(filename)
    remove_ok('temp/temp.txt', true)
    remove_ok(filename, true)
    remove_ok(filename, false)
end

function test_fs:test_remove_all()
    local function remove_all(path, n)
        lu.assertEquals(fs.exists(fs.path(path)), n ~= 0, path)
        lu.assertEquals(fs.remove_all(fs.path(path)), n, path)
        lu.assertIsFalse(fs.exists(fs.path(path)), path)
    end

    local filename = 'temp.txt'
    os.remove(filename)
    io.open(filename, 'w'):close()
    remove_all(filename, 1)
    remove_all(filename, 0)

    local filename = 'temp'
    fs.create_directories(fs.path(filename))
    remove_all(filename, 1)
    remove_all(filename, 0)

    local filename = 'temp/temp'
    fs.create_directories(fs.path(filename))
    remove_all(filename, 1)
    remove_all(filename, 0)
    
    local filename = 'temp'
    fs.create_directories(fs.path(filename))
    io.open('temp/temp.txt', 'w'):close()
    remove_all(filename, 2)
    remove_all(filename, 0)
end

function test_fs:test_is_directory()
    local function is_directory(path, b)
        lu.assertEquals(fs.is_directory(fs.path(path)), b, path)
    end
    local filename = 'temp.txt'
    is_directory('./test', true)
    is_directory('.', true)
    io.open(filename, 'w'):close()
    is_directory(filename, false)
    os.remove(filename)
    is_directory(filename, false)
end

function test_fs:test_create_directory()
    local function create_directory_ok(path, cb)
        local fspath = fs.path(path)
        fs.remove_all(fspath)
        lu.assertIsFalse(fs.exists(fs.path(path)), path)
        lu.assertIsTrue(fs.create_directory(fspath), path)
        lu.assertIsTrue(fs.is_directory(fspath), path)
        if cb then cb() end
        lu.assertIsTrue(fs.remove(fspath))
        lu.assertIsFalse(fs.exists(fs.path(path)), path)
    end
    local function create_directory_failed(path)
        local fspath = fs.path(path)
        fs.remove_all(fspath)
        lu.assertIsFalse(fs.exists(fs.path(path)), path)
        lu.assertError(fs.create_directory, fspath)
        lu.assertIsFalse(fs.is_directory(fspath), path)
        lu.assertIsFalse(fs.exists(fs.path(path)), path)
    end
    create_directory_ok('temp', function()
        create_directory_ok('temp/temp')
    end)
    create_directory_failed('temp/temp')
end

function test_fs:test_create_directories()
    local function create_directories_ok(path, cb)
        local fspath = fs.path(path)
        fs.remove_all(fspath)
        lu.assertIsFalse(fs.exists(fs.path(path)), path)
        lu.assertIsTrue(fs.create_directories(fspath), path)
        lu.assertIsTrue(fs.is_directory(fspath), path)
        if cb then cb() end
        lu.assertIsTrue(fs.remove(fspath))
        lu.assertIsFalse(fs.exists(fs.path(path)), path)
    end
    create_directories_ok('temp', function()
        create_directories_ok('temp/temp')
    end)
    create_directories_ok('temp/temp')
end

function test_fs:test_rename()
    local function rename_ok(from, to)
        lu.assertIsTrue(fs.exists(fs.path(from)), from)
        lu.assertIsFalse(fs.exists(fs.path(to)), to)
        fs.rename(fs.path(from), fs.path(to))
        lu.assertIsFalse(fs.exists(fs.path(from)), from)
        lu.assertIsTrue(fs.exists(fs.path(to)), to)
        fs.remove_all(fs.path(to))
        lu.assertIsFalse(fs.exists(fs.path(to)), to)
    end
    local function rename_failed(from, to)
        lu.assertError(fs.rename, fs.path(from), fs.path(to))
        fs.remove_all(fs.path(from))
        fs.remove_all(fs.path(to))
        lu.assertIsFalse(fs.exists(fs.path(from)), from)
        lu.assertIsFalse(fs.exists(fs.path(to)), to)
    end
    os.remove('temp1.txt')
    os.remove('temp2.txt')
    io.open('temp1.txt', 'w'):close()
    rename_ok('temp1.txt', 'temp2.txt')
    
    fs.remove_all(fs.path('temp1'))
    fs.remove_all(fs.path('temp2'))
    fs.create_directories(fs.path('temp1'))
    rename_ok('temp1', 'temp2')

    fs.remove_all(fs.path('temp1'))
    fs.remove_all(fs.path('temp2'))
    fs.create_directories(fs.path('temp1'))
    fs.create_directories(fs.path('temp2'))
    rename_failed('temp1', 'temp2')
end

function test_fs:test_current_path()
    lu.assertEquals(fs.current_path():string(), shell:pwd())
end

--[==[

-- list_directory
--TODO

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
