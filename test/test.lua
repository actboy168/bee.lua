local plat = ...

package.path = './test/?.lua'
package.cpath = ('./bin/%s/?.dll'):format(plat)

dofile './3rd/luaffi/src/test.lua'
require 'test_filesystem'
require 'test_thread'
require 'test_subprocess'
require 'test_registry'
print 'ok'
