local support = {
    msvc_x86_Debug = {
        OS = 'Windows',
        Arch = '32',
        Compiler = 'msvc',
        CRT = 'msvc',
        DEBUG = true,
    },
    msvc_x86_Release = {
        OS = 'Windows',
        Arch = '32',
        Compiler = 'msvc',
        CRT = 'msvc',
        DEBUG = false,
    },
    msvc_x64_Debug = {
        OS = 'Windows',
        Arch = '64',
        Compiler = 'msvc',
        CRT = 'msvc',
        DEBUG = true,
    },
    msvc_x64_Release = {
        OS = 'Windows',
        Arch = '64',
        Compiler = 'msvc',
        CRT = 'msvc',
        DEBUG = false,
    },
    mingw_Debug = {
        OS = 'Windows',
        Arch = '64',
        Compiler = 'gcc',
        CRT = 'mingw',
        DEBUG = true,
    },
    mingw_Release = {
        OS = 'Windows',
        Arch = '64',
        Compiler = 'gcc',
        CRT = 'mingw',
        DEBUG = false,
    },
}

local lu = require 'luaunit'
local platform = require 'bee.platform'

test_plat = {}

function test_plat:test_1()
    lu.assertNotNil(__Target__)
    lu.assertEquals(support[__Target__], platform)
end
