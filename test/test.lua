package.path = './test/?.lua'
package.cpath = './bin/x86/debug/?.dll'

dofile './3rd/luaffi/src/test.lua'
require 'test_filesystem'
require 'test_subprocess'
require 'test_registry'
print 'ok'
