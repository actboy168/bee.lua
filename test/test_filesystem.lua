require 'bee'
local platform = require 'bee.platform'
local fs = require 'bee.filesystem'
local lu = require 'luaunit'
local shell = require 'shell'

local C
local D
if platform.OS == 'Windows' then
    C = 'c:/'
    D = 'd:/'
else
    C = '/mnt/c/'
    D = '/mnt/d/'
end

local function create_file(filename, content)
    os.remove(filename)
    local f = assert(io.open(filename, 'wb'))
    if content ~= nil then
        f:write(content)
    end
    f:close()
end
local function read_file(filename)
    local f = assert(io.open(filename, 'rb'))
    local content = f:read 'a'
    f:close()
    return content
end

local test_fs = lu.test "filesystem"

local ALLOW_WRITE = 0x92
local USER_WRITE = 0x80

function test_fs:test_setup()
    if fs.exists(fs.path('temp1.txt')) then
        fs.path('temp1.txt'):add_permissions(ALLOW_WRITE)
        os.remove('temp1.txt')
    end
    if fs.exists(fs.path('temp2.txt')) then
        fs.path('temp2.txt'):add_permissions(ALLOW_WRITE)
        os.remove('temp2.txt')
    end
end

function test_fs:test_path()
    local path = fs.path('')
    lu.assertEquals(type(path) == 'userdata' or type(path) == 'table', true)
end

function test_fs:test_string()
    lu.assertEquals(fs.path('a/b'):string(), 'a/b')
    if platform.OS == 'Windows' then
        lu.assertEquals(fs.path('a\\b'):string(), 'a/b')
    end
end

function test_fs:test_filename()
    local function get_filename(path)
        return fs.path(path):filename():string()
    end
    lu.assertEquals(get_filename('a/b'), 'b')
    lu.assertEquals(get_filename('a/b/'), '')
    if platform.OS == 'Windows' then
        lu.assertEquals(get_filename('a\\b'), 'b')
        lu.assertEquals(get_filename('a\\b\\'), '')
    end
end

function test_fs:test_parent_path()
    local function get_parent_path(path)
        return fs.path(path):parent_path():string()
    end
    lu.assertEquals(get_parent_path('a/b/c'), 'a/b')
    lu.assertEquals(get_parent_path('a/b/'), 'a/b')
    if platform.OS == 'Windows' then
        lu.assertEquals(get_parent_path('a\\b\\c'), 'a/b')
        lu.assertEquals(get_parent_path('a\\b\\'), 'a/b')
    end
end

function test_fs:test_stem()
    local function get_stem(path)
        return fs.path(path):stem():string()
    end
    lu.assertEquals(get_stem('a/b/c.ext'), 'c')
    lu.assertEquals(get_stem('a/b/c'), 'c')
    lu.assertEquals(get_stem('a/b/.ext'), '.ext')
    if platform.OS == 'Windows' then
        lu.assertEquals(get_stem('a\\b\\c.ext'), 'c')
        lu.assertEquals(get_stem('a\\b\\c'), 'c')
        lu.assertEquals(get_stem('a\\b\\.ext'), '.ext')
    end
end

function test_fs:test_extension()
    local function get_extension(path)
        return fs.path(path):extension():string()
    end
    lu.assertEquals(get_extension('a/b/c.ext'), '.ext')
    lu.assertEquals(get_extension('a/b/c'), '')
    lu.assertEquals(get_extension('a/b/.ext'), '')
    if platform.OS == 'Windows' then
        lu.assertEquals(get_extension('a\\b\\c.ext'), '.ext')
        lu.assertEquals(get_extension('a\\b\\c'), '')
        lu.assertEquals(get_extension('a\\b\\.ext'), '')
    end

    lu.assertEquals(get_extension('a/b/c.'), '.')
    lu.assertEquals(get_extension('a/b/c..'), '.')
    lu.assertEquals(get_extension('a/b/c..lua'), '.lua')
end

function test_fs:test_absolute_relative()
    local function assertIsAbsolute(path)
        lu.assertEquals(fs.path(path):is_absolute(), true)
        lu.assertEquals(fs.path(path):is_relative(), false)
    end
    local function assertIsRelative(path)
        lu.assertEquals(fs.path(path):is_absolute(), false)
        lu.assertEquals(fs.path(path):is_relative(), true)
    end
    assertIsAbsolute(C..'a/b')
    if not (platform.OS == 'Windows' and platform.CRT == 'libstdc++') then
        -- TODO: mingw bug
        assertIsAbsolute('//a/b')
    end
    assertIsRelative('./a/b')
    assertIsRelative('a/b')
    assertIsRelative('../a/b')
    if platform.OS == 'Windows' then
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
    if platform.OS == 'Windows' then
        lu.assertEquals(remove_filename('a\\b\\c'), 'a/b/')
        lu.assertEquals(remove_filename('a\\b\\'), 'a/b/')
    end
end

function test_fs:test_replace_extension()
    local function replace_extension(path, ext)
        return fs.path(path):replace_extension(ext):string()
    end
    lu.assertEquals(replace_extension('a/b/c.ext', '.lua'), 'a/b/c.lua')
    lu.assertEquals(replace_extension('a/b/c', '.lua'), 'a/b/c.lua')
    lu.assertEquals(replace_extension('a/b/.ext', '.lua'), 'a/b/.ext.lua')
    if platform.OS == 'Windows' then
        lu.assertEquals(replace_extension('a\\b\\c.ext', '.lua'), 'a/b/c.lua')
        lu.assertEquals(replace_extension('a\\b\\c', '.lua'), 'a/b/c.lua')
        lu.assertEquals(replace_extension('a\\b\\.ext', '.lua'), 'a/b/.ext.lua')
    end

    lu.assertEquals(replace_extension('a/b/c.ext', 'lua'), 'a/b/c.lua')
    lu.assertEquals(replace_extension('a/b/c.ext', '..lua'), 'a/b/c..lua')
    lu.assertEquals(replace_extension('c.ext', '.lua'), 'c.lua')
end

function test_fs:test_equal_extension()
    local function equal_extension(path, ext)
        return lu.assertEquals(fs.path(path):equal_extension(ext), true)
    end
    equal_extension('a/b/c.ext', '.ext')
    equal_extension('a/b/c.ext', 'ext')
    equal_extension('a/b/c', '')
    equal_extension('a/b/.ext', '')
    equal_extension('a/b/c.', '.')
    equal_extension('a/b/c..', '.')
    equal_extension('a/b/c..lua', '.lua')
    equal_extension('a/b/c..lua', 'lua')
end

function test_fs:test_permissions()
    local filename = 'temp.txt'
    create_file(filename)

    lu.assertEquals(fs.path(filename):permissions() & USER_WRITE, USER_WRITE)
    shell:add_readonly(filename)
    lu.assertEquals(fs.path(filename):permissions() & USER_WRITE, 0)
    shell:del_readonly(filename)

    os.remove(filename)
end

function test_fs:test_add_remove_permissions()
    local filename = 'temp.txt'
    create_file(filename)

    lu.assertEquals(fs.path(filename):permissions() & USER_WRITE, USER_WRITE)
    fs.path(filename):remove_permissions(ALLOW_WRITE)
    lu.assertEquals(fs.path(filename):permissions() & USER_WRITE, 0)
    fs.path(filename):add_permissions(ALLOW_WRITE)
    lu.assertEquals(fs.path(filename):permissions() & USER_WRITE, USER_WRITE)

    os.remove(filename)
end

function test_fs:test_div()
    local function eq_div(A, B, C)
        lu.assertEquals(fs.path(A) / B, fs.path(C))
    end
    eq_div('a', 'b', 'a/b')
    eq_div('a/b', 'c', 'a/b/c')
    eq_div('a/', 'b', 'a/b')
    eq_div('a', '/b', '/b')
    eq_div('', 'a/b', 'a/b')
    eq_div(C..'a', D..'b', D..'b')
    eq_div(C..'a/', D..'b', D..'b')
    if platform.OS == 'Windows' then
        eq_div('a/', '\\b', '/b')
        eq_div('a\\b', 'c', 'a/b/c')
        eq_div('a\\', 'b', 'a/b')
        eq_div(C..'a', '/b', C..'b')
        eq_div(C..'a/', '\\b', C..'b')
    else
        eq_div('a/b', 'c', 'a/b/c')
        eq_div(C..'a', '/b', '/b')
        eq_div(C..'a/', '/b', '/b')
    end
end

function test_fs:test_concat()
    local function concat(a, b, c)
        lu.assertEquals(fs.path(a) .. b, fs.path(c))
        lu.assertEquals(fs.path(a) .. fs.path(b), fs.path(c))
    end
    concat('a', 'b', 'ab')
    concat('a/b', 'c', 'a/bc')
end

function test_fs:test_absolute()
    local function eq_absolute1(path)
        return lu.assertEquals(fs.absolute(fs.path(path)):string(), (fs.current_path() / path):string())
    end
    local function eq_absolute2(path1, path2)
        return lu.assertEquals(fs.absolute(fs.path(path1)):string(), (fs.current_path() / path2):string())
    end
    if platform.OS == 'Windows' then
        eq_absolute1('a\\')
        eq_absolute1('a\\b')
        eq_absolute1('a\\b\\')
        eq_absolute2('a/b', 'a\\b')
        eq_absolute2('./b', 'b')
        eq_absolute2('a/../b', 'b')
        eq_absolute2('a/b/../', 'a\\')
    else
        eq_absolute1('a/')
        eq_absolute1('a/b')
        eq_absolute1('a/b/')
        eq_absolute2('a/b', 'a/b')
        eq_absolute2('./b', 'b')
        eq_absolute2('a/../b', 'b')
        eq_absolute2('a/b/../', 'a/')
    end
end

function test_fs:test_relative()
    local function relative(a, b, c)
        return lu.assertEquals(fs.relative(fs.path(a), fs.path(b)):string(), c)
    end
    relative(C..'a/b/c', C..'a/b', 'c')
    relative(C..'a/b', C..'a/b/c',  '..')
    relative(C..'a/b/c', C..'a',  'b/c')
    relative(C..'a/d/e', C..'a/b/c',  '../../d/e')
    if platform.OS == 'Windows' then
        --relative(D..'a/b/c', C..'a',  '')
        relative('a', C..'a/b/c',  '')
        relative(C..'a/b', 'a/b/c',  '')
        relative(C..'a\\b\\c', C..'a',  'b/c')
        relative('c:\\a\\b\\c', 'C:\\a',  'b/c')
    else
        -- TODO
        --relative(D..'a/b/c', C..'a',  '')
        --relative('a', C..'a/b/c',  '')
        --relative(C..'a/b', 'a/b/c',  '')
    end
end

function test_fs:test_eq()
    local function eq(A, B)
        return lu.assertEquals(fs.path(A), fs.path(B))
    end
    eq('a/b', 'a/b')
    eq('a/./b', 'a/b')
    eq('a/b/../c', 'a/c')
    eq('a/b/../c', 'a/d/../c')
    if platform.OS == 'Windows' then
        eq('a/B', 'a/b')
        eq('a/b', 'a\\b')
    end
end

function test_fs:test_exists()
    local function is_exists(path, b)
        lu.assertEquals(fs.exists(fs.path(path)), b, path)
    end
    local filename = 'temp.txt'
    os.remove(filename)
    is_exists(filename, false)
    create_file(filename)
    is_exists(filename, true)
    os.remove(filename)
    is_exists(filename, false)
end

function test_fs:test_remove()
    local function remove_ok(path, b)
        lu.assertEquals(fs.exists(fs.path(path)), b)
        lu.assertEquals(fs.remove(fs.path(path)), b)
        lu.assertEquals(fs.exists(fs.path(path)), false)
    end
    local function remove_failed(path)
        lu.assertEquals(fs.exists(fs.path(path)), true)
        lu.assertError(fs.remove, fs.path(path))
        lu.assertEquals(fs.exists(fs.path(path)), true)
    end

    local filename = 'temp.txt'
    os.remove(filename)
    create_file(filename)
    remove_ok(filename, true)
    remove_ok(filename, false)

    local filename = 'temp'
    fs.remove_all(fs.path(filename))
    fs.create_directories(fs.path(filename))
    remove_ok(filename, true)
    remove_ok(filename, false)

    local filename = 'temp/temp'
    fs.remove_all(fs.path(filename))
    fs.create_directories(fs.path(filename))
    remove_ok(filename, true)
    remove_ok(filename, false)

    local filename = 'temp'
    fs.remove_all(fs.path(filename))
    fs.create_directories(fs.path(filename))
    create_file('temp/temp.txt')
    remove_failed(filename)
    remove_ok('temp/temp.txt', true)
    remove_ok(filename, true)
    remove_ok(filename, false)
end

function test_fs:test_remove_all()
    local function remove_all(path, n)
        lu.assertEquals(fs.exists(fs.path(path)), n ~= 0)
        lu.assertEquals(fs.remove_all(fs.path(path)), n)
        lu.assertEquals(fs.exists(fs.path(path)), false)
    end

    local filename = 'temp.txt'
    os.remove(filename)
    create_file(filename)
    remove_all(filename, 1)
    remove_all(filename, 0)

    local filename = 'temp'
    fs.remove_all(fs.path(filename))
    fs.create_directories(fs.path(filename))
    remove_all(filename, 1)
    remove_all(filename, 0)

    local filename = 'temp/temp'
    fs.remove_all(fs.path(filename))
    fs.create_directories(fs.path(filename))
    remove_all(filename, 1)
    remove_all(filename, 0)

    local filename = 'temp'
    fs.remove_all(fs.path(filename))
    fs.create_directories(fs.path(filename))
    create_file('temp/temp.txt')
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
    create_file(filename)
    is_directory(filename, false)
    os.remove(filename)
    is_directory(filename, false)
end

function test_fs:test_is_regular_file()
    local function is_regular_file(path, b)
        lu.assertEquals(fs.is_regular_file(fs.path(path)), b, path)
    end
    local filename = 'temp.txt'
    is_regular_file('./test', false)
    is_regular_file('.', false)
    create_file(filename)
    is_regular_file(filename, true)
    os.remove(filename)
    is_regular_file(filename, false)
end

function test_fs:test_create_directory()
    local function create_directory_ok(path, cb)
        local fspath = fs.path(path)
        fs.remove_all(fspath)
        lu.assertEquals(fs.exists(fs.path(path)), false)
        lu.assertEquals(fs.create_directory(fspath), true)
        lu.assertEquals(fs.is_directory(fspath), true)
        if cb then cb() end
        lu.assertEquals(fs.remove(fspath), true)
        lu.assertEquals(fs.exists(fs.path(path)), false)
    end
    local function create_directory_failed(path)
        local fspath = fs.path(path)
        fs.remove_all(fspath)
        lu.assertEquals(fs.exists(fs.path(path)), false)
        lu.assertError(fs.create_directory, fspath)
        lu.assertEquals(fs.is_directory(fspath), false)
        lu.assertEquals(fs.exists(fs.path(path)), false)
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
        lu.assertEquals(fs.exists(fs.path(path)), false)
        lu.assertEquals(fs.create_directories(fspath), true)
        lu.assertEquals(fs.is_directory(fspath), true)
        if cb then cb() end
        lu.assertEquals(fs.remove(fspath), true)
        lu.assertEquals(fs.exists(fs.path(path)), false)
    end
    create_directories_ok('temp', function()
        create_directories_ok('temp/temp')
    end)
    create_directories_ok('temp/temp')
end

function test_fs:test_rename()
    local function rename_ok(from, to)
        lu.assertEquals(fs.exists(fs.path(from)), true)
        fs.rename(fs.path(from), fs.path(to))
        lu.assertEquals(fs.exists(fs.path(from)), false)
        lu.assertEquals(fs.exists(fs.path(to)), true)
        fs.remove_all(fs.path(to))
        lu.assertEquals(fs.exists(fs.path(to)), false)
    end
    local function rename_failed(from, to)
        lu.assertError(fs.rename, fs.path(from), fs.path(to))
        fs.remove_all(fs.path(from))
        fs.remove_all(fs.path(to))
        lu.assertEquals(fs.exists(fs.path(from)), false)
        lu.assertEquals(fs.exists(fs.path(to)), false)
    end
    os.remove('temp1.txt')
    os.remove('temp2.txt')
    create_file('temp1.txt')
    rename_ok('temp1.txt', 'temp2.txt')

    fs.remove_all(fs.path('temp1'))
    fs.remove_all(fs.path('temp2'))
    fs.create_directories(fs.path('temp1'))
    rename_ok('temp1', 'temp2')

    fs.remove_all(fs.path('temp1'))
    fs.remove_all(fs.path('temp2'))
    fs.create_directories(fs.path('temp1'))
    fs.create_directories(fs.path('temp2'))
    if platform.OS == 'Windows' then
        rename_failed('temp1', 'temp2')
    else
        rename_ok('temp1', 'temp2')
    end

    fs.remove_all(fs.path('temp1'))
    fs.remove_all(fs.path('temp2'))
    fs.create_directories(fs.path('temp1'))
    fs.create_directories(fs.path('temp2'))
    create_file('temp2/temp.txt')
    rename_failed('temp1', 'temp2')
end

function test_fs:test_current_path()
    lu.assertEquals(fs.current_path(), fs.path(shell:pwd()))
end

function test_fs:test_copy_file()
    local function copy_file_ok(from, to)
        lu.assertEquals(fs.exists(fs.path(from)), true)
        lu.assertEquals(fs.exists(fs.path(to)), true)
        lu.assertEquals(read_file(from), read_file(to))
        fs.remove_all(fs.path(from))
        fs.remove_all(fs.path(to))
        lu.assertEquals(fs.exists(fs.path(from)), false)
        lu.assertEquals(fs.exists(fs.path(to)), false)
    end
    local function copy_file_failed(from, to)
        fs.remove_all(fs.path(from))
        fs.remove_all(fs.path(to))
        lu.assertEquals(fs.exists(fs.path(from)), false)
        lu.assertEquals(fs.exists(fs.path(to)), false)
    end
    for _, copy in ipairs {fs.copy_file, fs.copy} do
        create_file('temp1.txt', tostring(os.time()))
        os.remove('temp2.txt')
        copy(fs.path 'temp1.txt', fs.path 'temp2.txt', false)
        copy_file_ok('temp1.txt', 'temp2.txt')

        create_file('temp1.txt', tostring(os.time()))
        os.remove('temp2.txt')
        copy(fs.path 'temp1.txt', fs.path 'temp2.txt', true)
        copy_file_ok('temp1.txt', 'temp2.txt')

        if fs.copy ~= copy or platform.CRT == "msvc" then
            create_file('temp1.txt', tostring(os.time()))
            create_file('temp2.txt', tostring(os.clock()))
            copy(fs.path 'temp1.txt', fs.path 'temp2.txt', true)
            copy_file_ok('temp1.txt', 'temp2.txt')
        end

        create_file('temp1.txt', tostring(os.time()))
        create_file('temp2.txt', tostring(os.clock()))
        lu.assertError(copy, fs.path 'temp1.txt', fs.path 'temp2.txt')
        copy_file_failed('temp1.txt', 'temp2.txt')
    end

    create_file('temp1.txt', tostring(os.time()))
    create_file('temp2.txt', tostring(os.clock()))
    lu.assertEquals(fs.path('temp1.txt'):permissions() & USER_WRITE, USER_WRITE)
    fs.path('temp1.txt'):remove_permissions(ALLOW_WRITE)
    lu.assertEquals(fs.path('temp1.txt'):permissions() & USER_WRITE, 0)
    lu.assertEquals(fs.path('temp2.txt'):permissions() & USER_WRITE, USER_WRITE)
    fs.copy_file(fs.path('temp1.txt'), fs.path('temp2.txt'), true)
    lu.assertEquals(fs.exists(fs.path('temp1.txt')), true)
    lu.assertEquals(fs.exists(fs.path('temp2.txt')), true)
    lu.assertEquals(fs.path('temp2.txt'):permissions() & USER_WRITE, 0)
    lu.assertEquals(read_file('temp1.txt'), read_file('temp2.txt'))
    fs.path('temp1.txt'):add_permissions(ALLOW_WRITE)
    fs.path('temp2.txt'):add_permissions(ALLOW_WRITE)
    os.remove('temp1.txt')
    os.remove('temp2.txt')
    lu.assertEquals(fs.exists(fs.path('temp1.txt')), false)
    lu.assertEquals(fs.exists(fs.path('temp2.txt')), false)
end

function test_fs:test_list_directory()
    local function list_directory_ok(dir, expected)
        local result = {}
        for path in fs.path(dir):list_directory() do
            result[path:string()] = true
        end
        lu.assertEquals(result, expected)
        fs.remove_all(fs.path(dir))
    end
    local function list_directory_failed(dir)
        local fsdir = fs.path(dir)
        lu.assertError(fsdir.list_directory, fsdir)
    end

    fs.create_directories(fs.path('temp'))
    create_file('temp/temp1.txt')
    create_file('temp/temp2.txt')
    list_directory_ok('temp', {
        ['temp/temp1.txt'] = true,
        ['temp/temp2.txt'] = true,
    })

    fs.create_directories(fs.path('temp/temp'))
    create_file('temp/temp1.txt')
    create_file('temp/temp2.txt')
    create_file('temp/temp/temp1.txt')
    create_file('temp/temp/temp2.txt')
    list_directory_ok('temp', {
        ['temp/temp1.txt'] = true,
        ['temp/temp2.txt'] = true,
        ['temp/temp'] = true,
    })

    fs.remove_all(fs.path('temp.txt'))
    fs.remove_all(fs.path('temp'))
    list_directory_failed('temp.txt')
    list_directory_failed('temp')
    list_directory_failed('temp.txt')
    create_file('temp.txt')
    list_directory_failed('temp.txt')
    fs.remove_all(fs.path('temp.txt'))
end

function test_fs:test_copy_dir()
    local function list_directory(dir, result)
        result = result or {}
        for path in fs.path(dir):list_directory() do
            if fs.is_directory(path) then
                list_directory(path, result)
            end
            result[path:string()] = true
        end
        return result
    end
    local function file_equals(a, b)
        lu.assertEquals(read_file(a), read_file(b))
    end

    fs.create_directories(fs.path('temp/temp'))
    create_file('temp/temp1.txt', tostring(math.random()))
    create_file('temp/temp2.txt', tostring(math.random()))
    create_file('temp/temp/temp1.txt', tostring(math.random()))
    create_file('temp/temp/temp2.txt', tostring(math.random()))

    fs.copy(fs.path('temp'), fs.path('temp1'), true)

    lu.assertEquals(list_directory('temp'), {
        ['temp/temp1.txt'] = true,
        ['temp/temp2.txt'] = true,
        ['temp/temp/temp1.txt'] = true,
        ['temp/temp/temp2.txt'] = true,
        ['temp/temp'] = true,
    })
    lu.assertEquals(list_directory('temp1'), {
        ['temp1/temp1.txt'] = true,
        ['temp1/temp2.txt'] = true,
        ['temp1/temp/temp1.txt'] = true,
        ['temp1/temp/temp2.txt'] = true,
        ['temp1/temp'] = true,
    })
    file_equals('temp/temp1.txt', 'temp1/temp1.txt')
    file_equals('temp/temp2.txt', 'temp1/temp2.txt')
    file_equals('temp/temp/temp1.txt', 'temp1/temp/temp1.txt')
    file_equals('temp/temp/temp2.txt', 'temp1/temp/temp2.txt')

    fs.remove_all(fs.path('temp'))
    fs.remove_all(fs.path('temp1'))
end

function test_fs:test_last_write_time()
    local function last_write_time(filename)
        os.remove(filename)
        local t1 = os.time()
        create_file(filename)
        local tf = fs.last_write_time(fs.path(filename))
        local t2 = os.time()
        os.remove(filename)
        lu.assertEquals(tf >= t1 - 10, true)
        lu.assertEquals(tf <= t2 + 10, true)
    end
    last_write_time('temp.txt')
end

function test_fs:test_exe_path()
    local function getexe()
        local i = 0
        while arg[i] ~= nil do
            i = i - 1
        end
        return fs.absolute(fs.path(arg[i + 1])):string()
    end
    lu.assertEquals(fs.exe_path():string(), getexe())
end

function test_fs:test_dll_path()
    local function getdll()
        local i = 0
        while arg[i] ~= nil do
            i = i - 1
        end
        return fs.absolute(fs.path(arg[i + 1]):parent_path() / ('bee.' .. __EXT__))
    end
    lu.assertEquals(fs.dll_path(), getdll())
end

function test_fs:test_filelock_1()
    local lock = fs.path("temp.lock")
    local f1, err1 = fs.filelock(lock)
    lu.assertIsUserdata(f1, err1)
    lu.assertEquals(fs.filelock(lock), nil)
    f1:close()
    local f2, err2 = fs.filelock(lock)
    lu.assertIsUserdata(f2, err2)
    f2:close()
    fs.remove(fs.path("temp.lock"))
end

function test_fs:test_filelock_2()
    local subprocess = require "bee.subprocess"
    local function getexe()
        local i = 0
        while arg[i] ~= nil do
            i = i - 1
        end
        return arg[i + 1]
    end
    local function createLua(script, option)
        local init = ("package.cpath = [[%s]]"):format(package.cpath)
        option = option or {}
        option[1] = {
            getexe(),
            '-e', init,
            '-e', script,
        }
        return subprocess.spawn(option)
    end

    local process = createLua([[
        local fs = require 'bee.filesystem'
        fs.filelock(fs.path("temp.lock"))
        io.stdout:write 'ok'
        io.stdout:flush()
        io.read 'a'
    ]], { stdin = true, stdout = true, stderr = true })
    lu.assertEquals(process.stdout:read(2), 'ok')
    lu.assertEquals(fs.filelock(fs.path("temp.lock")), nil)
    process.stdin:close()
    lu.assertEquals(process.stderr:read 'a', '')
    lu.assertEquals(process:wait(), 0)
    local f, err = fs.filelock(fs.path("temp.lock"))
    lu.assertIsUserdata(f, err)
    f:close()
    fs.remove(fs.path("temp.lock"))
end

function test_fs:test_tostring()
    local function test(s)
        lu.assertEquals(fs.path(s):string(), s)
        lu.assertEquals(tostring(fs.path(s)), s)
    end
    test ""
    test "简体中文"
    test "繁體中文"
    test "日本語"
    test "한국어"
    test "العربية"
    test "עברית"
    test "🤣🤪"
end
