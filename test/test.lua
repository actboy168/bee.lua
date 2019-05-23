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

local lu = require 'luaunit'

require 'test_lua'
require 'test_platform'
require 'test_serialization'
require 'test_filesystem'
require 'test_thread'
require 'test_subprocess'
require 'test_socket'
require 'test_filewatch'
--require 'test_registry'

local code = lu.LuaUnit.run()

if platform.OS ~= "Windows" then
    collectgarbage "collect"
    local ls = require "bee.socket"
    local sock = assert(ls.bind("tcp", "127.0.0.1", 0))
    local fd = tostring(sock):gsub("socket %((%d+)%)", "%1")
    sock:close()
    assert(tonumber(fd) == 3)
end

os.exit(code, true)
