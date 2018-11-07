local fs = require 'bee.filesystem'
local uni = require 'ffi.unicode'

local function wait_second()
    local f = io.popen('ping -n 1 127.1>nul', 'r')
    f:read 'a'
    f:close()
end

-- fs.path
local path = fs.path('test')
assert(type(path) == 'userdata')

-- string
local path = fs.path('test')
assert(path:string() == 'test')

-- filename
local path = fs.path('dir/filename')
assert(path:filename():string() == 'filename')

-- parent_path
local path = fs.path('dir/filename')
assert(path:parent_path():string() == 'dir')

-- stem
local path = fs.path('dir/filename.lua')
assert(path:stem():string() == 'filename')

-- extension
local path = fs.path('dir/filename.lua')
assert(path:extension():string() == '.lua')

-- is_absolute
local path = fs.path('dir/filename.lua')
assert(path:is_absolute() == false)
local path = fs.path('D:/dir/filename.lua')
assert(path:is_absolute() == true)

-- is_relative
local path = fs.path('dir/filename.lua')
assert(path:is_relative() == true)
local path = fs.path('D:/dir/filename.lua')
assert(path:is_relative() == false)

-- remove_filename
local path = fs.path('dir/filename.lua')
assert(path:remove_filename():string() == 'dir/')

-- replace_extension
local path = fs.path('dir/filename.lua')
local ext = '.json'
assert(path:replace_extension(ext):string() == 'dir/filename.json')

local path = fs.path('dir/filename.lua')
local ext = fs.path('.json')
assert(path:replace_extension(ext):string() == 'dir/filename.json')

-- list_directory
local path = fs.path('C:/')
local founded = {}
for filename in path:list_directory() do
    local string = filename:filename():string()
    founded[string] = true
end

local os_list = io.popen([[dir C:\ /A /B]], 'r'):read 'a'
for filename in os_list:gmatch '[^\r\n]+' do
    filename = uni.a2u(filename)
    assert(founded[filename] == true)
    founded[filename] = nil
end
assert(next(founded) == nil)

local ALLOW_WRITE = 0x92

-- permissions
local path = fs.path('temp.txt')
io.open(path:string(), 'w'):close()
assert(path:permissions() & ALLOW_WRITE ~= 0)
os.execute('attrib +r temp.txt')
assert(path:permissions() & ALLOW_WRITE == 0)
os.execute('attrib -r temp.txt')
os.remove(path:string())

-- add_permissions
local path = fs.path('temp.txt')
io.open(path:string(), 'w'):close()
assert(path:permissions() & ALLOW_WRITE ~= 0)
os.execute('attrib +r temp.txt')
assert(path:permissions() & ALLOW_WRITE == 0)
path:add_permissions(ALLOW_WRITE)
os.remove(path:string())

-- remove_permissions
local path = fs.path('temp.txt')
io.open(path:string(), 'w'):close()
assert(path:permissions() & ALLOW_WRITE ~= 0)
path:remove_permissions(ALLOW_WRITE)
assert(path:permissions() & ALLOW_WRITE == 0)
os.execute('attrib -r temp.txt')
os.remove(path:string())

-- __div
local path = fs.path('dir') / 'filename'
assert(path:string() == 'dir\\filename')

-- __eq
assert(fs.path('test') == fs.path('test'))

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
    wait_second()
    os.execute('rd /Q ' .. dir:string())
end

-- create_directories
local dir = fs.path('tempdir')
local dirs = dir / 'a' / 'b'
fs.create_directories(dirs)
assert(fs.is_directory(dir) == true)
assert(fs.is_directory(dirs) == true)
while fs.exists(dir) do
    wait_second()
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
