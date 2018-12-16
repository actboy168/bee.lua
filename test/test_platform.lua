local support = {}
for _, config in ipairs {'Debug', 'Release'} do
    support['msvc_x86_' .. config] = {
        OS = 'Windows',
        Arch = '32',
        Compiler = 'msvc',
        CRT = 'msvc',
        DEBUG = (config == 'Debug'),
    }
    support['msvc_x64_' .. config] = {
        OS = 'Windows',
        Arch = '64',
        Compiler = 'msvc',
        CRT = 'msvc',
        DEBUG = (config == 'Debug'),
    }
    support['mingw_' .. config] = {
        OS = 'Windows',
        Arch = '64',
        Compiler = 'gcc',
        CRT = 'mingw',
        DEBUG = (config == 'Debug'),
    }
    support['linux_' .. config] = {
        OS = 'Linux',
        Arch = '64',
        Compiler = 'gcc',
        CRT = 'glibc',
        DEBUG = (config == 'Debug'),
    }
end

local lu = require 'luaunit'
local platform = require 'bee.platform'

test_plat = {}

function test_plat:test_1()
    lu.assertNotNil(__Target__)
    lu.assertEquals(support[__Target__], platform)
end
