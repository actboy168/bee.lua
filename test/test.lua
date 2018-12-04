package.path = './test/?.lua'
package.cpath = './bin/msvc_x86_debug/?.dll'

dofile './3rd/luaffi/src/test.lua'
require 'test_filesystem'
require 'test_subprocess'
require 'test_registry'
require 'test_thread'
print 'ok'
