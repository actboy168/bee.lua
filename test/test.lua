local function getProcDir()
    local i = 0
    while arg[i] ~= nil do
        i = i - 1
    end
    return arg[i + 1]:match("(.+)[/\\][%w_.-]+$")
end
local function getTestDir()
    return arg[0]:match("(.+)[/\\][%w_.-]+$")
end
__Target__ = getProcDir()
__EXT__ = package.cpath:match '[/\\]%?%.([a-z]+)'
package.path = table.concat({
    getTestDir() .. '/?.lua',
    getTestDir() .. '/ltest/?.lua',
}, ";")
package.cpath = ('%s/?.%s'):format(__Target__, __EXT__)

local platform = require 'bee.platform'
local lt = require 'ltest'

if not lt.options.list then
    local v = platform.os_version
    print("OS:       ", ("%s %d.%d.%d"):format(platform.os, v.major, v.minor, v.revision))
    print("Arch:     ", platform.Arch)
    print("Compiler: ", platform.CompilerVersion)
    print("CRT:      ", platform.CRTVersion)
    print("DEBUG:    ", platform.DEBUG)
end

--require 'test_lua'
require 'test_serialization'
require 'test_filesystem'
require 'test_thread'
require 'test_subprocess'
require 'test_socket'
require 'test_filewatch'
require 'test_time'

do
    local fs = require 'bee.filesystem'
    if lt.options.touch then
        lt.options.touch = fs.absolute(lt.options.touch):string()
    end
    local tmpdir = fs.temp_directory_path() / "test_bee"
    fs.create_directories(tmpdir)
    fs.current_path(tmpdir)
end

os.exit(lt.run(), true)
