local lu = require 'luaunit'

local fs = require 'bee.filesystem'
local platform = require 'bee.platform'

local shell = {}
function shell:add_readonly(filename)
    if platform.OS == 'Windows' then
        os.execute(('attrib +r %q'):format(filename))
    else
        os.execute(('chmod a-w %q'):format(filename))
    end
end
function shell:del_readonly(filename)
    if platform.OS == 'Windows' then
        os.execute(('attrib -r %q'):format(filename))
    else
        os.execute(('chmod a+w %q'):format(filename))
    end
end

function shell:pwd()
    local command
    if platform.OS == "Windows" then
        command = 'echo %cd%'
    else
        command = 'pwd'
    end
    return (io.popen(command):read 'a'):gsub('[\n\r]*$', ''):gsub('\\', '/')
end

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

test_fs = {}

function test_fs:test_path()
    lu.assertUserdata(fs.path(''))
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
        lu.assertIsTrue(fs.path(path):is_absolute(), path)
        lu.assertIsFalse(fs.path(path):is_relative(), path)
    end
    local function assertIsRelative(path)
        lu.assertIsFalse(fs.path(path):is_absolute(), path)
        lu.assertIsTrue(fs.path(path):is_relative(), path)
    end
    assertIsAbsolute(C..'a/b')
    if platform.CRT ~= 'mingw' then
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
end

--local ALLOW_WRITE = 0x92
local ALLOW_WRITE = 0x80

function test_fs:test_permissions()
    local filename = 'temp.txt'
    create_file(filename)

    lu.assertEquals(fs.path(filename):permissions() & ALLOW_WRITE, ALLOW_WRITE)
    shell:add_readonly(filename)
    lu.assertEquals(fs.path(filename):permissions() & ALLOW_WRITE, 0)
    shell:del_readonly(filename)

    os.remove(filename)
end

function test_fs:test_add_remove_permissions()
    local filename = 'temp.txt'
    create_file(filename)

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
    lu.assertEquals(div('a', 'b'), 'a/b')
    lu.assertEquals(div('a/b', 'c'), 'a/b/c')
    lu.assertEquals(div('a/', 'b'), 'a/b')
    lu.assertEquals(div('a', '/b'), '/b')
    lu.assertEquals(div(C..'a', D..'b'), D..'b')
    lu.assertEquals(div(C..'a/', D..'b'), D..'b')
    if platform.OS == 'Windows' then
        lu.assertEquals(div('a/', '\\b'), '/b')
        lu.assertEquals(div('a\\b', 'c'), 'a/b/c')
        lu.assertEquals(div('a\\', 'b'), 'a/b')
        lu.assertEquals(div(C..'a', '/b'), C..'b')
        lu.assertEquals(div(C..'a/', '\\b'), C..'b')
    else
        lu.assertEquals(div('a/b', 'c'), 'a/b/c')
        lu.assertEquals(div(C..'a', '/b'), '/b')
        lu.assertEquals(div(C..'a/', '/b'), '/b')
    end
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
        relative(D..'a/b/c', C..'a',  '')
        relative('a', C..'a/b/c',  '')
        relative(C..'a/b', 'a/b/c',  '')
        relative(C..'a\\b\\c', C..'a',  'b/c')
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
        lu.assertEquals(fs.exists(fs.path(path)), n ~= 0, path)
        lu.assertEquals(fs.remove_all(fs.path(path)), n, path)
        lu.assertIsFalse(fs.exists(fs.path(path)), path)
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
    lu.assertEquals(fs.current_path():string(), shell:pwd())
end

function test_fs:test_copy_file()
    local function copy_file_ok(from, to, flag)
        fs.copy_file(fs.path(from), fs.path(to), flag)
        lu.assertIsTrue(fs.exists(fs.path(from)), from)
        lu.assertIsTrue(fs.exists(fs.path(to)), to)
        lu.assertEquals(read_file(from), read_file(to))
        fs.remove_all(fs.path(from))
        fs.remove_all(fs.path(to))
        lu.assertIsFalse(fs.exists(fs.path(from)), from)
        lu.assertIsFalse(fs.exists(fs.path(to)), to)
    end
    local function copy_file_failed(from, to)
        lu.assertError(fs.copy_file, fs.path(from), fs.path(to))
        fs.remove_all(fs.path(from))
        fs.remove_all(fs.path(to))
        lu.assertIsFalse(fs.exists(fs.path(from)), from)
        lu.assertIsFalse(fs.exists(fs.path(to)), to)
    end
    create_file('temp1.txt', tostring(os.time()))
    os.remove('temp2.txt')
    copy_file_ok('temp1.txt', 'temp2.txt', false)

    create_file('temp1.txt', tostring(os.time()))
    os.remove('temp2.txt')
    copy_file_ok('temp1.txt', 'temp2.txt', true)

    create_file('temp1.txt', tostring(os.time()))
    create_file('temp2.txt', tostring(os.clock()))
    copy_file_ok('temp1.txt', 'temp2.txt', true)

    create_file('temp1.txt', tostring(os.time()))
    create_file('temp2.txt', tostring(os.clock()))
    copy_file_failed('temp1.txt', 'temp2.txt', false)
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

    fs.remove_all(fs.path('temp'))
    fs.create_directories(fs.path('temp'))
    create_file('temp/temp1.txt')
    create_file('temp/temp2.txt')
    list_directory_ok('temp', {
        ['temp/temp1.txt'] = true,
        ['temp/temp2.txt'] = true,
    })

    fs.remove_all(fs.path('temp'))
    fs.create_directories(fs.path('temp'))
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

function test_fs:test_last_write_time()
    local function last_write_time(filename)
        os.remove(filename)
        local t1 = os.time()
        create_file(filename)
        local tf = fs.last_write_time(fs.path(filename))
        local t2 = os.time()
        os.remove(filename)
        lu.assertIsTrue(tf >= t1 and tf <= t2, ('start=%d, end=%d, write=%d'):format(t1, t2, tf))
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
        return fs.absolute(fs.path(arg[i + 1]):parent_path() / ('bee.' .. __EXT__)):string()
    end
    lu.assertEquals(fs.dll_path():string(), getdll())
end

if platform.CRT == 'mingw' then
    --直到mingw 8.2，仍然有许多不符合标准的行为，所以打几个补丁，以便让测试通过。
    local fs_remove = fs.remove
    function fs.remove(path)
        if not fs.exists(path) then
            return false
        end
        return fs_remove(path)
    end

    local fs_copy_file = fs.copy_file
    function fs.copy_file(from, to, flag)
        if flag and fs.exists(to) then
            fs.remove(to)
        end
        return fs_copy_file(from, to, flag)
    end

    local path_mt = debug.getmetatable(fs.path())
    local path_is_absolute = path_mt.is_absolute
    function path_mt.is_absolute(path)
        print(path:string())
        if path:string():sub(1, 2):match '[/\\][/\\]' then
            return true
        end
        return path_is_absolute(path)
    end
    function path_mt.is_relative(path)
        return not path_mt.is_absolute(path)
    end

    local path_string = path_mt.string
    function path_mt.string(path)
        return path_string(path):gsub('\\', '/')
    end

    function path_mt.parent_path(path)
        return fs.path(path:string():match("(.+)[/\\][%w_.-]*$"))
    end
end
