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
    print("OS:       ", platform.OS)
    print("Arch:     ", platform.Arch)
    print("Compiler: ", platform.CompilerVersion)
    print("CRT:      ", platform.CRTVersion)
    print("DEBUG:    ", platform.DEBUG)
end

--local function fd_count()
--    local ls = require "bee.socket"
--    local sock = assert(ls.bind("tcp", "127.0.0.1", 0))
--    local fd = tostring(sock):gsub("socket %((%d+)%)", "%1")
--    sock:close()
--    return tonumber(fd)
--end
--local initfd = fd_count()

require 'test_lua'
require 'test_serialization'
require 'test_filesystem'
require 'test_thread'
require 'test_subprocess'
require 'test_socket'
require 'test_filewatch'
require 'test_time'

local success = lt.run()

--if platform.OS ~= "Windows" then
--    collectgarbage "collect"
--    local fd = fd_count()
--    assert(fd == initfd, "init cout = "..initfd..", fd count = " .. fd)
--end

os.exit(success, true)
