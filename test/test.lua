local plat = ...

package.path = './test/?.lua'
package.cpath = ('./bin/%s/?.dll'):format(plat)

if plat:sub(1, 4) == "msvc" then
    dofile './3rd/luaffi/src/test.lua'
end

require 'test_filesystem'
require 'test_thread'
require 'test_subprocess'
require 'test_registry'
print 'ok'
