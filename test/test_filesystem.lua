local fs = require 'bee.filesystem'

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

-- __eq
--assert(fs.path('test') == fs.path('test'))
