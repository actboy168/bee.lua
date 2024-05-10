local platform  = require "bee.platform"
local fs        = require "bee.filesystem"
local lt        = require "ltest"
local shell     = require "shell"
local supported = require "supported"

local isWindows = platform.os == "windows"
local isMinGW   = isWindows and platform.CRT == "libstdc++"
local isMacOS   = platform.os ~= "macos"

local C <const> = isWindows and "c:/" or "/mnt/c/"
local D <const> = isWindows and "d:/" or "/mnt/d/"

local function create_file(filename, content)
    if type(filename) == "userdata" then
        filename = filename:string()
    end
    fs.remove(filename)
    local f = assert(io.open(filename, "wb"))
    if content ~= nil then
        f:write(content)
    end
    f:close()
end
local function read_file(filename)
    if type(filename) == "userdata" then
        filename = filename:string()
    end
    local f = assert(io.open(filename, "rb"))
    local content = f:read "a"
    f:close()
    return content
end

local test_fs = lt.test "filesystem"

local ALLOW_WRITE <const> = 0x92
local USER_WRITE <const> = 0x80

function test_fs:test_setup()
    if fs.exists(fs.path("temp1.txt")) then
        fs.permissions(fs.path("temp1.txt"), ALLOW_WRITE)
        fs.remove("temp1.txt")
    end
    if fs.exists(fs.path("temp2.txt")) then
        fs.permissions(fs.path("temp2.txt"), ALLOW_WRITE)
        fs.remove("temp2.txt")
    end
end

function test_fs:test_path()
    local path = fs.path("")
    lt.assertEquals(type(path) == "userdata" or type(path) == "table", true)
end

function test_fs:test_string()
    lt.assertEquals(fs.path("a/b"):string(), "a/b")
    if isWindows then
        lt.assertEquals(fs.path("a\\b"):string(), "a/b")
    end
end

function test_fs:test_filename()
    local function get_filename(path)
        return fs.path(path):filename():string()
    end
    lt.assertEquals(get_filename("a/b"), "b")
    lt.assertEquals(get_filename("a/b/"), "")
    if isWindows then
        lt.assertEquals(get_filename("a\\b"), "b")
        lt.assertEquals(get_filename("a\\b\\"), "")
    end
end

function test_fs:test_parent_path()
    local function get_parent_path(path)
        return fs.path(path):parent_path():string()
    end
    lt.assertEquals(get_parent_path("a/b/c"), "a/b")
    lt.assertEquals(get_parent_path("a/b/"), "a/b")
    if isWindows then
        lt.assertEquals(get_parent_path("a\\b\\c"), "a/b")
        lt.assertEquals(get_parent_path("a\\b\\"), "a/b")
    end
end

function test_fs:test_stem()
    local function get_stem(path)
        return fs.path(path):stem():string()
    end
    lt.assertEquals(get_stem("a/b/c.ext"), "c")
    lt.assertEquals(get_stem("a/b/c"), "c")
    lt.assertEquals(get_stem("a/b/.ext"), ".ext")
    if isWindows then
        lt.assertEquals(get_stem("a\\b\\c.ext"), "c")
        lt.assertEquals(get_stem("a\\b\\c"), "c")
        lt.assertEquals(get_stem("a\\b\\.ext"), ".ext")
    end
end

function test_fs:test_extension()
    local function get_extension(path)
        return fs.path(path):extension()
    end
    lt.assertEquals(get_extension("a/b/c.ext"), ".ext")
    lt.assertEquals(get_extension("a/b/c"), "")
    lt.assertEquals(get_extension("a/b/.ext"), "")
    if isWindows then
        lt.assertEquals(get_extension("a\\b\\c.ext"), ".ext")
        lt.assertEquals(get_extension("a\\b\\c"), "")
        lt.assertEquals(get_extension("a\\b\\.ext"), "")
    end

    lt.assertEquals(get_extension("a/b/c."), ".")
    lt.assertEquals(get_extension("a/b/c.."), ".")
    lt.assertEquals(get_extension("a/b/c..lua"), ".lua")
end

function test_fs:test_absolute_relative()
    local function assertIsAbsolute(path)
        lt.assertEquals(fs.path(path):is_absolute(), true)
        lt.assertEquals(fs.path(path):is_relative(), false)
    end
    local function assertIsRelative(path)
        lt.assertEquals(fs.path(path):is_absolute(), false)
        lt.assertEquals(fs.path(path):is_relative(), true)
    end
    assertIsAbsolute(C.."a/b")
    if not isMinGW then
        -- TODO: mingw bug
        assertIsAbsolute("//a/b")
    end
    assertIsRelative("./a/b")
    assertIsRelative("a/b")
    assertIsRelative("../a/b")
    if isWindows then
        assertIsRelative("/a/b")
    else
        assertIsAbsolute("/a/b")
    end
end

function test_fs:test_remove_filename()
    local function remove_filename(path)
        return fs.path(path):remove_filename():string()
    end
    lt.assertEquals(remove_filename("a/b/c"), "a/b/")
    lt.assertEquals(remove_filename("a/b/"), "a/b/")
    if isWindows then
        lt.assertEquals(remove_filename("a\\b\\c"), "a/b/")
        lt.assertEquals(remove_filename("a\\b\\"), "a/b/")
    end
end

function test_fs:test_replace_filename()
    local function replace_filename(path, ext)
        return fs.path(path):replace_filename(ext):string()
    end
    lt.assertEquals(replace_filename("a/b/c.lua", "d.lua"), "a/b/d.lua")
    lt.assertEquals(replace_filename("a/b/c", "d.lua"), "a/b/d.lua")
    lt.assertEquals(replace_filename("a/b/", "d.lua"), "a/b/d.lua")
    if isWindows then
        lt.assertEquals(replace_filename("a\\b\\c.lua", "d.lua"), "a/b/d.lua")
        lt.assertEquals(replace_filename("a\\b\\c", "d.lua"), "a/b/d.lua")
        lt.assertEquals(replace_filename("a\\b\\", "d.lua"), "a/b/d.lua")
    end
end

function test_fs:test_replace_extension()
    local function replace_extension(path, ext)
        return fs.path(path):replace_extension(ext):string()
    end
    lt.assertEquals(replace_extension("a/b/c.ext", ".lua"), "a/b/c.lua")
    lt.assertEquals(replace_extension("a/b/c", ".lua"), "a/b/c.lua")
    lt.assertEquals(replace_extension("a/b/.ext", ".lua"), "a/b/.ext.lua")
    if isWindows then
        lt.assertEquals(replace_extension("a\\b\\c.ext", ".lua"), "a/b/c.lua")
        lt.assertEquals(replace_extension("a\\b\\c", ".lua"), "a/b/c.lua")
        lt.assertEquals(replace_extension("a\\b\\.ext", ".lua"), "a/b/.ext.lua")
    end

    lt.assertEquals(replace_extension("a/b/c.ext", "lua"), "a/b/c.lua")
    lt.assertEquals(replace_extension("a/b/c.ext", "..lua"), "a/b/c..lua")
    lt.assertEquals(replace_extension("c.ext", ".lua"), "c.lua")
end

function test_fs:test_get_permissions()
    local filename = "temp.txt"
    create_file(filename)

    lt.assertEquals(fs.permissions(fs.path(filename)) & USER_WRITE, USER_WRITE)
    shell:add_readonly(filename)
    lt.assertEquals(fs.permissions(fs.path(filename)) & USER_WRITE, 0)
    shell:del_readonly(filename)

    fs.remove(filename)
end

function test_fs:test_set_permissions()
    local filename = fs.path "temp.txt"
    create_file(filename)

    lt.assertEquals(fs.permissions(filename) & USER_WRITE, USER_WRITE)
    fs.permissions(filename, ALLOW_WRITE, fs.perm_options.remove)
    lt.assertEquals(fs.permissions(filename) & USER_WRITE, 0)
    fs.permissions(filename, ALLOW_WRITE, fs.perm_options.add)
    lt.assertEquals(fs.permissions(filename) & USER_WRITE, USER_WRITE)
    fs.permissions(filename, 0)
    lt.assertEquals(fs.permissions(filename) & USER_WRITE, 0)
    fs.permissions(filename, ALLOW_WRITE)
    lt.assertEquals(fs.permissions(filename) & USER_WRITE, USER_WRITE)

    fs.remove(filename:string())
end

function test_fs:test_div()
    local function eq_div(a, b, c)
        lt.assertEquals(fs.path(a) / b, fs.path(c))
    end
    eq_div("a", "b", "a/b")
    eq_div("a/b", "c", "a/b/c")
    eq_div("a/", "b", "a/b")
    eq_div("a", "/b", "/b")
    eq_div("", "a/b", "a/b")
    eq_div(C.."a", D.."b", D.."b")
    eq_div(C.."a/", D.."b", D.."b")
    if isWindows then
        eq_div("a/", "\\b", "/b")
        eq_div("a\\b", "c", "a/b/c")
        eq_div("a\\", "b", "a/b")
        eq_div(C.."a", "/b", C.."b")
        eq_div(C.."a/", "\\b", C.."b")
    else
        eq_div("a/b", "c", "a/b/c")
        eq_div(C.."a", "/b", "/b")
        eq_div(C.."a/", "/b", "/b")
    end
end

function test_fs:test_concat()
    local function concat(a, b, c)
        lt.assertEquals(fs.path(a)..b, fs.path(c))
        lt.assertEquals(fs.path(a)..fs.path(b), fs.path(c))
    end
    concat("a", "b", "ab")
    concat("a/b", "c", "a/bc")
end

function test_fs:test_lexically_normal()
    local function test(path1, path2)
        return lt.assertEquals(fs.path(path1):lexically_normal():string(), path2)
    end
    test("a/", "a/")
    test("a/b", "a/b")
    test("./b", "b")
    test("a/b/../", "a/")
    test("a/b/c/../../d", "a/d")
    if isWindows then
        test("a\\", "a/")
        test("a\\b", "a/b")
        test(".\\b", "b")
        test("a\\b\\..\\", "a/")
        test("a\\b\\c\\..\\..\\d", "a/d")
    end
end

function test_fs:test_absolute()
    local function eq_absolute2(path1, path2)
        return lt.assertEquals(fs.absolute(fs.path(path1)), fs.current_path() / path2)
    end
    local function eq_absolute1(path)
        return eq_absolute2(path, path)
    end
    eq_absolute1("a/")
    eq_absolute1("a/b")
    eq_absolute1("a/b/")
    eq_absolute2("a/b", "a/b")
    eq_absolute2("./b", "b")
    eq_absolute2("a/../b", "b")
    eq_absolute2("a/b/../", "a/")
    eq_absolute2("a/b/c/../../d", "a/d")
end

function test_fs:test_relative()
    local function relative(a, b, c)
        return lt.assertEquals(fs.relative(fs.path(a), fs.path(b)):string(), c)
    end
    relative(C.."a/b/c", C.."a/b", "c")
    relative(C.."a/b", C.."a/b/c", "..")
    relative(C.."a/b/c", C.."a", "b/c")
    relative(C.."a/d/e", C.."a/b/c", "../../d/e")
    if isWindows then
        --relative(D.."a/b/c", C.."a",  "")
        relative("a", C.."a/b/c", "")
        relative(C.."a/b", "a/b/c", "")
        relative(C.."a\\b\\c", C.."a", "b/c")
    else
        -- TODO
        --relative(D.."a/b/c", C.."a",  "")
        --relative("a", C.."a/b/c",  "")
        --relative(C.."a/b", "a/b/c",  "")
    end
end

function test_fs:test_eq()
    local function eq(A, B)
        return lt.assertEquals(fs.path(A), fs.path(B))
    end
    eq("a/b", "a/b")
    eq("a/./b", "a/b")
    eq("a/b/../c", "a/c")
    eq("a/b/../c", "a/d/../c")
    if isWindows then
        eq("a/B", "a/b")
        eq("a/b", "a\\b")
    end
end

function test_fs:test_exists()
    local function is_exists(path, b)
        lt.assertEquals(fs.exists(fs.path(path)), b)
    end
    local filename = "temp.txt"
    fs.remove(filename)
    is_exists(filename, false)

    create_file(filename)
    is_exists(filename, true)
    if not isMinGW then
        -- TODO: mingw bug
        is_exists(filename.."/", false)
    end
    is_exists(filename.."/"..filename, false)

    fs.remove(filename)
    is_exists(filename, false)
    is_exists(filename.."/"..filename, false)
end

function test_fs:test_remove()
    local function remove_ok(path, b)
        lt.assertEquals(fs.exists(fs.path(path)), b)
        lt.assertEquals(fs.remove(fs.path(path)), b)
        lt.assertEquals(fs.exists(fs.path(path)), false)
    end
    local function remove_failed(path)
        lt.assertEquals(fs.exists(fs.path(path)), true)
        lt.assertError(fs.remove, fs.path(path))
        lt.assertEquals(fs.exists(fs.path(path)), true)
    end

    fs.remove "temp.txt"
    create_file "temp.txt"
    remove_ok("temp.txt", true)
    remove_ok("temp.txt", false)

    fs.remove_all "temp"
    fs.create_directories "temp"
    remove_ok("temp", true)
    remove_ok("temp", false)

    fs.remove_all "temp/temp"
    fs.create_directories "temp/temp"
    remove_ok("temp/temp", true)
    remove_ok("temp/temp", false)

    fs.remove_all "temp"
    fs.create_directories "temp"
    create_file("temp/temp.txt")
    remove_failed "temp"
    remove_ok("temp/temp.txt", true)
    remove_ok("temp", true)
    remove_ok("temp", false)
end

function test_fs:test_remove_all()
    local function remove_all(path, n)
        lt.assertEquals(fs.exists(fs.path(path)), n ~= 0)
        lt.assertEquals(fs.remove_all(fs.path(path)), n)
        lt.assertEquals(fs.exists(fs.path(path)), false)
    end

    fs.remove "temp.txt"
    create_file "temp.txt"
    remove_all("temp.txt", 1)
    remove_all("temp.txt", 0)

    fs.remove_all "temp"
    fs.create_directories "temp"
    remove_all("temp", 1)
    remove_all("temp", 0)

    fs.remove_all "temp/temp"
    fs.create_directories "temp/temp"
    remove_all("temp/temp", 1)
    remove_all("temp/temp", 0)

    fs.remove_all "temp"
    fs.create_directories "temp"
    create_file("temp/temp.txt")
    remove_all("temp", 2)
    remove_all("temp", 0)
end

function test_fs:test_is_directory()
    local function is_directory(path, b)
        lt.assertEquals(fs.is_directory(fs.path(path)), b)
    end
    local filename = "temp.txt"
    is_directory(".", true)
    create_file(filename)
    is_directory(filename, false)
    fs.remove(filename)
    is_directory(filename, false)
end

function test_fs:test_is_regular_file()
    local function is_regular_file(path, b)
        lt.assertEquals(fs.is_regular_file(fs.path(path)), b)
    end
    local filename = "temp.txt"
    is_regular_file(".", false)
    create_file(filename)
    is_regular_file(filename, true)
    fs.remove(filename)
    is_regular_file(filename, false)
end

function test_fs:test_create_directory()
    local function create_directory_ok(path, cb)
        local fspath = fs.path(path)
        fs.remove_all(fspath)
        lt.assertEquals(fs.exists(fs.path(path)), false)
        lt.assertEquals(fs.create_directory(fspath), true)
        lt.assertEquals(fs.is_directory(fspath), true)
        if cb then cb() end
        lt.assertEquals(fs.remove(fspath), true)
        lt.assertEquals(fs.exists(fs.path(path)), false)
    end
    local function create_directory_failed(path)
        local fspath = fs.path(path)
        fs.remove_all(fspath)
        lt.assertEquals(fs.exists(fs.path(path)), false)
        lt.assertError(fs.create_directory, fspath)
        lt.assertEquals(fs.is_directory(fspath), false)
        lt.assertEquals(fs.exists(fs.path(path)), false)
    end
    create_directory_ok("temp", function ()
        create_directory_ok("temp/temp")
    end)
    create_directory_failed("temp/temp")
end

function test_fs:test_create_directories()
    local function create_directories_ok(path, cb)
        local fspath = fs.path(path)
        fs.remove_all(fspath)
        lt.assertEquals(fs.exists(fs.path(path)), false)
        lt.assertEquals(fs.create_directories(fspath), true)
        lt.assertEquals(fs.is_directory(fspath), true)
        if cb then cb() end
        lt.assertEquals(fs.remove(fspath), true)
        lt.assertEquals(fs.exists(fs.path(path)), false)
    end
    create_directories_ok("temp", function ()
        create_directories_ok("temp/temp")
    end)
    create_directories_ok("temp/temp")
end

function test_fs:test_rename()
    local function rename_ok(from, to)
        lt.assertEquals(fs.exists(fs.path(from)), true)
        fs.rename(fs.path(from), fs.path(to))
        lt.assertEquals(fs.exists(fs.path(from)), false)
        lt.assertEquals(fs.exists(fs.path(to)), true)
        fs.remove_all(fs.path(to))
        lt.assertEquals(fs.exists(fs.path(to)), false)
    end
    local function rename_failed(from, to)
        lt.assertError(fs.rename, fs.path(from), fs.path(to))
        fs.remove_all(fs.path(from))
        fs.remove_all(fs.path(to))
        lt.assertEquals(fs.exists(fs.path(from)), false)
        lt.assertEquals(fs.exists(fs.path(to)), false)
    end
    fs.remove("temp1.txt")
    fs.remove("temp2.txt")
    create_file("temp1.txt")
    rename_ok("temp1.txt", "temp2.txt")

    fs.remove_all(fs.path("temp1"))
    fs.remove_all(fs.path("temp2"))
    fs.create_directories(fs.path("temp1"))
    rename_ok("temp1", "temp2")

    fs.remove_all(fs.path("temp1"))
    fs.remove_all(fs.path("temp2"))
    fs.create_directories(fs.path("temp1"))
    fs.create_directories(fs.path("temp2"))
    if isWindows then
        rename_failed("temp1", "temp2")
    else
        rename_ok("temp1", "temp2")
    end

    fs.remove_all(fs.path("temp1"))
    fs.remove_all(fs.path("temp2"))
    fs.create_directories(fs.path("temp1"))
    fs.create_directories(fs.path("temp2"))
    create_file("temp2/temp.txt")
    rename_failed("temp1", "temp2")
end

function test_fs:test_current_path()
    lt.assertEquals(fs.current_path(), fs.path(shell:pwd()))
end

function test_fs:test_copy_file()
    local function copy_file_ok(from, to)
        lt.assertEquals(fs.exists(fs.path(from)), true)
        lt.assertEquals(fs.exists(fs.path(to)), true)
        lt.assertEquals(read_file(from), read_file(to))
        fs.remove_all(fs.path(from))
        fs.remove_all(fs.path(to))
        lt.assertEquals(fs.exists(fs.path(from)), false)
        lt.assertEquals(fs.exists(fs.path(to)), false)
    end
    local function copy_file_failed(from, to)
        fs.remove_all(fs.path(from))
        fs.remove_all(fs.path(to))
        lt.assertEquals(fs.exists(fs.path(from)), false)
        lt.assertEquals(fs.exists(fs.path(to)), false)
    end
    for _, copy in ipairs { fs.copy_file, fs.copy } do
        local NONE = fs.copy_options.none
        local OVERWRITE = fs.copy_options.overwrite_existing
        if copy == fs.copy then
            NONE = NONE | fs.copy_options.recursive
            OVERWRITE = OVERWRITE | fs.copy_options.recursive
        end
        create_file("temp1.txt", tostring(os.time()))
        fs.remove("temp2.txt")
        copy(fs.path "temp1.txt", fs.path "temp2.txt", NONE)
        copy_file_ok("temp1.txt", "temp2.txt")

        create_file("temp1.txt", tostring(os.time()))
        fs.remove("temp2.txt")
        copy(fs.path "temp1.txt", fs.path "temp2.txt", OVERWRITE)
        copy_file_ok("temp1.txt", "temp2.txt")

        if fs.copy ~= copy or platform.CRT == "msvc" then
            create_file("temp1.txt", tostring(os.time()))
            create_file("temp2.txt", tostring(os.clock()))
            copy(fs.path "temp1.txt", fs.path "temp2.txt", OVERWRITE)
            copy_file_ok("temp1.txt", "temp2.txt")
        end

        create_file("temp1.txt", tostring(os.time()))
        create_file("temp2.txt", tostring(os.clock()))
        lt.assertError(copy, fs.path "temp1.txt", fs.path "temp2.txt", NONE)
        copy_file_failed("temp1.txt", "temp2.txt")
    end

    create_file("temp1.txt", tostring(os.time()))
    create_file("temp2.txt", tostring(os.clock()))
    lt.assertEquals(fs.permissions(fs.path "temp1.txt") & USER_WRITE, USER_WRITE)
    fs.permissions(fs.path("temp1.txt"), ALLOW_WRITE, fs.perm_options.remove)
    lt.assertEquals(fs.permissions(fs.path("temp1.txt")) & USER_WRITE, 0)
    lt.assertEquals(fs.permissions(fs.path("temp2.txt")) & USER_WRITE, USER_WRITE)
    fs.copy_file(fs.path("temp1.txt"), fs.path("temp2.txt"), fs.copy_options.overwrite_existing)
    lt.assertEquals(fs.exists(fs.path("temp1.txt")), true)
    lt.assertEquals(fs.exists(fs.path("temp2.txt")), true)
    lt.assertEquals(fs.permissions(fs.path("temp2.txt")) & USER_WRITE, 0)
    lt.assertEquals(read_file("temp1.txt"), read_file("temp2.txt"))
    fs.permissions(fs.path("temp1.txt"), ALLOW_WRITE, fs.perm_options.add)
    fs.permissions(fs.path("temp2.txt"), ALLOW_WRITE, fs.perm_options.add)
    fs.remove("temp1.txt")
    fs.remove("temp2.txt")
    lt.assertEquals(fs.exists(fs.path("temp1.txt")), false)
    lt.assertEquals(fs.exists(fs.path("temp2.txt")), false)
end

function test_fs:test_copy_file_2()
    local from        = fs.path "temp1.txt"
    local to          = fs.path "temp2.txt"
    local FromContext = tostring(os.time())
    local ToContext   = tostring(os.clock())
    local COPIED
    --copy_options::none
    create_file(from, FromContext)
    create_file(to, ToContext)
    lt.assertError(fs.copy_file, from, to, fs.copy_options.none)
    lt.assertEquals(ToContext, read_file(to))
    --copy_options::skip_existing
    COPIED = fs.copy_file(from, to, fs.copy_options.skip_existing)
    lt.assertEquals(COPIED, false)
    lt.assertEquals(ToContext, read_file(to))
    --copy_options::overwrite_existing
    COPIED = fs.copy_file(from, to, fs.copy_options.overwrite_existing)
    lt.assertEquals(COPIED, true)
    lt.assertEquals(FromContext, read_file(to))
    --copy_options::update_existing
    create_file(from, FromContext)
    create_file(to, ToContext)
    COPIED = fs.copy_file(from, to, fs.copy_options.update_existing)
    lt.assertEquals(COPIED, false)
    lt.assertEquals(ToContext, read_file(to))

    --TODO: gcc实现的文件写时间精度太低
    --TODO: macos暂时还需要兼容低版本
    --create_file(to,   ToContext)
    --create_file(from, FromContext)
    --COPIED = fs.copy_file(from, to, fs.copy_options.update_existing)
    --lu.assertEquals(COPIED, true)
    --lu.assertEquals(FromContext, read_file(to))
    --clean
    fs.remove(from:string())
    fs.remove(to:string())
    lt.assertEquals(fs.exists(from), false)
    lt.assertEquals(fs.exists(to), false)
end

function test_fs:test_pairs()
    local function pairs_ok(dir, pairs, expected)
        local fsdir = fs.path(dir)
        local result = {}
        for path, status in pairs(fsdir) do
            result[path:string()] = status:type()
        end
        lt.assertEquals(result, expected)
    end
    local function pairs_failed(dir)
        local fsdir = fs.path(dir)
        lt.assertError(fs.pairs, fsdir)
    end

    fs.remove_all "temp"
    fs.create_directories "temp"
    create_file("temp/temp1.txt")
    create_file("temp/temp2.txt")
    pairs_ok("temp", fs.pairs, {
        ["temp/temp1.txt"] = "regular",
        ["temp/temp2.txt"] = "regular",
    })
    fs.remove_all "temp"

    fs.create_directories "temp/temp"
    create_file("temp/temp1.txt")
    create_file("temp/temp2.txt")
    create_file("temp/temp/temp1.txt")
    create_file("temp/temp/temp2.txt")
    pairs_ok("temp", fs.pairs, {
        ["temp/temp1.txt"] = "regular",
        ["temp/temp2.txt"] = "regular",
        ["temp/temp"] = "directory",
    })
    pairs_ok("temp", fs.pairs_r, {
        ["temp/temp1.txt"] = "regular",
        ["temp/temp2.txt"] = "regular",
        ["temp/temp"] = "directory",
        ["temp/temp/temp1.txt"] = "regular",
        ["temp/temp/temp2.txt"] = "regular",
    })

    fs.remove_all(fs.path("temp"))
    pairs_failed("temp.txt")
    pairs_failed("temp")
    pairs_failed("temp.txt")
    create_file("temp.txt")
    pairs_failed("temp.txt")
    fs.remove_all(fs.path("temp.txt"))
end

function test_fs:test_copy_dir()
    local function each_directory(dir, result)
        result = result or {}
        for path, status in fs.pairs(fs.path(dir)) do
            if status:is_directory() then
                each_directory(path, result)
            end
            result[path:string()] = true
        end
        return result
    end
    local function file_equals(a, b)
        lt.assertEquals(read_file(a), read_file(b))
    end

    fs.remove_all "temp"
    fs.create_directories(fs.path("temp/temp"))
    create_file("temp/temp1.txt", tostring(math.random()))
    create_file("temp/temp2.txt", tostring(math.random()))
    create_file("temp/temp/temp1.txt", tostring(math.random()))
    create_file("temp/temp/temp2.txt", tostring(math.random()))

    fs.copy(fs.path("temp"), fs.path("temp1"), fs.copy_options.overwrite_existing | fs.copy_options.recursive)

    lt.assertEquals(each_directory("temp"), {
        ["temp/temp1.txt"] = true,
        ["temp/temp2.txt"] = true,
        ["temp/temp/temp1.txt"] = true,
        ["temp/temp/temp2.txt"] = true,
        ["temp/temp"] = true,
    })
    lt.assertEquals(each_directory("temp1"), {
        ["temp1/temp1.txt"] = true,
        ["temp1/temp2.txt"] = true,
        ["temp1/temp/temp1.txt"] = true,
        ["temp1/temp/temp2.txt"] = true,
        ["temp1/temp"] = true,
    })
    file_equals("temp/temp1.txt", "temp1/temp1.txt")
    file_equals("temp/temp2.txt", "temp1/temp2.txt")
    file_equals("temp/temp/temp1.txt", "temp1/temp/temp1.txt")
    file_equals("temp/temp/temp2.txt", "temp1/temp/temp2.txt")

    fs.remove_all(fs.path("temp"))
    fs.remove_all(fs.path("temp1"))
end

function test_fs:test_last_write_time()
    local function last_write_time(filename)
        fs.remove(filename)
        create_file(filename)
        local t1 = fs.last_write_time(filename)
        fs.last_write_time(filename, t1)
        local t2 = fs.last_write_time(filename)
        fs.remove(filename)
        lt.assertEquals(t1, t2)
    end
    last_write_time("temp.txt")
end

function test_fs:test_tostring()
    local function test(s)
        lt.assertEquals(fs.path(s):string(), s)
        lt.assertEquals(tostring(fs.path(s)), s)
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

function test_fs:test_canonical()
    --local function test(a, b)
    --    lt.assertEquals(fs.canonical(fs.path(a)):string(), fs.absolute(fs.path(b)):string())
    --end
    --create_file "ABCabc.txt"
    --if isWindows and not isMinGW then
    --    test("abcabc.txt", "ABCabc.txt")
    --end
    --test("ABCabc.txt", "ABCabc.txt")
    --fs.remove "ABCabc.txt"
end

function test_fs:test_status()
    local function test_status(type, createf)
        local path = fs.path("temp."..type)
        fs.remove_all(path)
        createf(path)
        lt.assertEquals(fs.status(path):type(), type)
        fs.remove_all(path)
    end
    test_status("not_found", function () end)
    test_status("regular", create_file)
    test_status("directory", fs.create_directories)
end

function test_fs:test_symlink()
    if not supported "symlink" then
        return
    end
    local function test_create(createf, target, link)
        fs.remove_all(link)
        createf(target, link)
        lt.assertEquals(fs.status(target), fs.status(link))
        lt.assertEquals(fs.exists(target), fs.exists(link))
        lt.assertEquals(fs.is_directory(target), fs.is_directory(link))
        lt.assertEquals(fs.is_regular_file(target), fs.is_regular_file(link))
        lt.assertEquals(fs.status(target), fs.symlink_status(target))
        lt.assertEquals(fs.symlink_status(link):type(), "symlink")
    end
    local function test_remove(target, link)
        fs.remove_all(target)
        lt.assertEquals(fs.status(target):type(), "not_found")
        lt.assertEquals(fs.symlink_status(target):type(), "not_found")
        if isMacOS then
            lt.assertEquals(fs.status(link):type(), "not_found")
        end
        lt.assertEquals(fs.symlink_status(link):type(), "symlink")
        fs.remove_all(link)
        lt.assertEquals(fs.status(link):type(), "not_found")
        lt.assertEquals(fs.symlink_status(link):type(), "not_found")
    end

    do
        local target = fs.path "temp.txt"
        local link = fs.path "temp.link"
        fs.remove_all(target)
        local content = os.date()
        create_file(target, content)
        test_create(fs.create_symlink, target, link)
        lt.assertEquals(read_file(target), content)
        test_remove(target, link)
    end

    do
        local target = fs.path "tempdir"
        local link = fs.path "tempdir.link"
        fs.remove_all(target)
        fs.create_directories(target)
        test_create(fs.create_directory_symlink, target, link)
        local content = os.date()
        create_file(target / "temp.txt", content)
        lt.assertEquals(read_file(link / "temp.txt"), content)
        test_remove(target, link)
    end
end

function test_fs:test_hard_link()
    if not supported "hardlink" then
        return
    end
    local function test_create(createf, target, link)
        fs.remove_all(link)
        createf(target, link)
        lt.assertEquals(fs.status(target), fs.symlink_status(target))
        lt.assertEquals(fs.status(link), fs.symlink_status(link))
        lt.assertEquals(fs.status(link), fs.status(target))
    end
    local function test_remove(target, link)
        fs.remove_all(target)
        lt.assertEquals(fs.status(target):type(), "not_found")
        lt.assertEquals(fs.symlink_status(target):type(), "not_found")
        lt.assertNotEquals(fs.status(link):type(), "not_found")
        lt.assertNotEquals(fs.symlink_status(link):type(), "not_found")
        fs.remove_all(link)
        lt.assertEquals(fs.status(link):type(), "not_found")
        lt.assertEquals(fs.symlink_status(link):type(), "not_found")
    end
    local target = fs.path "temphard.txt"
    local link = fs.path "temphard.link"
    fs.remove_all(target)
    local content = os.date()
    create_file(target, content)
    test_create(fs.create_hard_link, target, link)
    lt.assertEquals(read_file(target), content)
    test_remove(target, link)
end

function test_fs:test_file_size()
    fs.remove_all "temp1.txt"
    create_file("temp1.txt", "1234567890")
    lt.assertEquals(fs.file_size "temp1.txt", 10)
    fs.remove_all "temp1.txt"
end
