package.cpath = '../bin/x86/debug/?.dll'
require 'bee_compat'
print(require 'bee.subprocess')
print(require 'bee.filesystem')
print(require 'bee.registry')
require 'test_filesystem'
require 'test_subprocess'
require 'test_registry'
print 'ok'
