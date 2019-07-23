local function getProcDir()
    local i = 0
    while arg[i] ~= nil do
        i = i - 1
    end
    return arg[i + 1]:match("(.+)[/\\][%w_.-]+$")
end
__Target__ = getProcDir()
__EXT__ = package.cpath:match '[/\\]%?%.([a-z]+)'
package.path = './test/?.lua'
package.cpath = ('%s/?.%s'):format(__Target__, __EXT__)

local platform = require 'bee.platform'
--if platform.Compiler == 'msvc' then
--    dofile './3rd/luaffi/src/test.lua'
--end

local function fd_count()
    local ls = require "bee.socket"
    local sock = assert(ls.bind("tcp", "127.0.0.1", 0))
    local fd = tostring(sock):gsub("socket %((%d+)%)", "%1")
    sock:close()
    return tonumber(fd)
end

local initfd = fd_count()

local lu = require 'luaunit'

debug.setCstacklimit(1000)

require 'test_lua'
require 'test_platform'
require 'test_serialization'
require 'test_filesystem'
require 'test_thread'
require 'test_subprocess'
require 'test_socket'
require 'test_filewatch'

if platform.OS == "Windows" then
    test_socket_1 = {UDS = true}
    test_socket_2 = {UDS = false}
    for k, v in pairs(test_socket) do
        test_socket_1[k] = v
        test_socket_2[k] = v
    end
    test_socket = nil
end

--require 'test_registry'

local code = lu.LuaUnit.run()

if platform.OS ~= "Windows" then
    collectgarbage "collect"
    local fd = fd_count()
    assert(fd == initfd, "fd count = " .. fd)
end

os.exit(code, true)
