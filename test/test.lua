local function getTarget()
    local i = 0
    while arg[i] ~= nil do
        i = i - 1
    end
    return arg[i + 1]:match("(.+)[/\\][%w_.-]+$"):match("[/\\]?([%w_.-]+)$")
end
__Target__ = getTarget()
__EXT__ = package.cpath:match '[/\\]%?%.([a-z]+)'
package.path = './test/?.lua'
package.cpath = ('./bin/%s/?.%s'):format(__Target__, __EXT__)

local platform = require 'bee.platform'
if platform.Compiler == 'msvc' then
    dofile './3rd/luaffi/src/test.lua'
end

local lu = require 'luaunit'

require 'test_lua'
require 'test_platform'
require 'test_serialization'
if platform.OS ~= 'macOS' then
    require 'test_filesystem'
end
require 'test_thread'
require 'test_subprocess'
require 'test_socket'
if platform.OS ~= 'Linux' then
require 'test_filewatch'
end
--require 'test_registry'

os.exit(lu.LuaUnit.run(), true)
