local function getTarget()
    local i = 0
    while arg[i] ~= nil do
        i = i - 1
    end
    return arg[i + 1]:match("(.+)[/\\][%w_.-]+$"):match("[/\\]?([%w_.-]+)$")
end
__Target__ = getTarget()

package.path = './test/?.lua'
package.cpath = ('./bin/%s/?.dll'):format(__Target__)

local platform = require 'bee.platform'
if platform.Compiler == 'msvc' then
    dofile './3rd/luaffi/src/test.lua'
end

local lu = require 'luaunit'

require 'test_platform'
require 'test_serialization'
require 'test_filesystem'
--require 'test_thread'
--require 'test_subprocess'
--require 'test_registry'

os.exit(lu.LuaUnit.run(), true)
