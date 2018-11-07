local registry = require 'bee.registry'

local reg = registry.open [[HKEY_CURRENT_USER\Environment]]
assert(reg ~= nil)
assert(reg['path'] ~= nil)
assert(reg['path']:find([[Microsoft VS Code\bin]], 1, true))

local reg = registry.open [[HKEY_CURRENT_USER\Software\bee_test]]
assert(reg ~= nil)
reg['TEST_REG_SZ']        = { registry.REG_SZ,        'bee' }
reg['TEST_REG_BINARY']    = { registry.REG_BINARY,    'BEE' }
reg['TEST_REG_DWORD']     = { registry.REG_DWORD,     10 }
reg['TEST_REG_QWORD']     = { registry.REG_QWORD,     2^50 }
reg['TEST_REG_MULTI_SZ']  = { registry.REG_MULTI_SZ,  'REG_MULTI_SZ' }
reg['TEST_REG_EXPAND_SZ'] = { registry.REG_EXPAND_SZ, 'REG_EXPAND_SZ' }

local reg = registry.open [[HKEY_CURRENT_USER\Software\bee_test]]
assert(reg ~= nil)
assert(reg['TEST_REG_SZ']        == 'bee')
assert(reg['TEST_REG_BINARY']    == 'BEE')
assert(reg['TEST_REG_DWORD']     == 10)
assert(reg['TEST_REG_QWORD']     == 2^50)
assert(reg['TEST_REG_MULTI_SZ']  == 'REG_MULTI_SZ\0')
assert(reg['TEST_REG_EXPAND_SZ'] == 'REG_EXPAND_SZ')

local reg = (registry.open [[HKEY_CURRENT_USER\Software]]) / 'bee_test'
assert(reg ~= nil)
assert(reg['TEST_REG_SZ']        == 'bee')
assert(reg['TEST_REG_BINARY']    == 'BEE')
assert(reg['TEST_REG_DWORD']     == 10)
assert(reg['TEST_REG_QWORD']     == 2^50)
assert(reg['TEST_REG_MULTI_SZ']  == 'REG_MULTI_SZ\0')
assert(reg['TEST_REG_EXPAND_SZ'] == 'REG_EXPAND_SZ')

local reg = registry.open [[HKEY_CURRENT_USER\Software\bee_test]]
assert(true == registry.del(reg))

local reg = registry.open [[HKEY_CURRENT_USER\Software\bee_test]]
-- 不存在的树是否可以打开？
assert(reg ~= nil)
assert(reg['TEST_REG_SZ']        == nil)
assert(reg['TEST_REG_BINARY']    == nil)
assert(reg['TEST_REG_DWORD']     == nil)
assert(reg['TEST_REG_QWORD']     == nil)
assert(reg['TEST_REG_MULTI_SZ']  == nil)
assert(reg['TEST_REG_EXPAND_SZ'] == nil)
