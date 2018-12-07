local platform, configuration = ...

package.path = './test/?.lua'
package.cpath = ('./bin/msvc_%s_%s/?.dll'):format(platform, configuration)

dofile './3rd/luaffi/src/test.lua'
require 'test_filesystem'
require 'test_thread'
require 'test_subprocess'
require 'test_registry'
print 'ok'
