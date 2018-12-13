local function getplat()
    local i = 0
    while arg[i] ~= nil do
        i = i - 1
    end
    return arg[i + 1]:match("(.+)[/\\][%w_.-]+$"):match("[/\\]?([%w_.-]+)$")
end

local plat = getplat()

package.path = './test/?.lua'
package.cpath = ('./bin/%s/?.dll'):format(plat)

if plat:sub(1, 4) == "msvc" then
    dofile './3rd/luaffi/src/test.lua'
end

require 'test_serialization'
require 'test_filesystem'
require 'test_thread'
require 'test_subprocess'
require 'test_registry'
print 'ok'
