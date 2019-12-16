local support = {}
for _, config in ipairs {'debug', 'release'} do
    support['msbuild_x86_' .. config] = {
        OS = 'Windows',
        Arch = '32',
        Compiler = 'msvc',
        CRT = 'msvc',
        DEBUG = (config == 'debug'),
    }
    support['msbuild_x64_' .. config] = {
        OS = 'Windows',
        Arch = '64',
        Compiler = 'msvc',
        CRT = 'msvc',
        DEBUG = (config == 'Debug'),
    }
end

support.msvc = {
    OS = 'Windows',
    Arch = '32',
    Compiler = 'msvc',
    CRT = 'msvc',
    DEBUG = false,
}
support.mingw = {
    OS = 'Windows',
    Arch = '64',
    Compiler = 'gcc',
    CRT = 'libstdc++',
    DEBUG = false,
}
support.linux = {
    OS = 'Linux',
    Arch = '64',
    Compiler = 'clang',
    CRT = 'libstdc++',
    DEBUG = false,
}
support.macos = {
    OS = 'macOS',
    Arch = '64',
    Compiler = 'clang',
    CRT = 'libc++',
    DEBUG = false,
}

local lu = require 'luaunit'
local fs = require 'bee.filesystem'
local platform = require 'bee.platform'

test_plat = {}

function test_plat:test_1()
    lu.assertNotNil(__Target__)
    local plat = fs.path(__Target__):parent_path():filename():string():lower()
    if plat == 'linux' then
        lu.assertIsTrue(platform.Compiler == 'gcc' or platform.Compiler == 'clang')
        support.linux.Compiler = platform.Compiler
    end
    if plat == 'msvc' then
        lu.assertIsTrue(platform.Arch == '32' or platform.Arch == '64')
        support.msvc.Arch = platform.Arch
    end
    local info = {}
    for k, v in pairs(platform) do
        info[k] = v
    end
    info.CompilerVersion = nil
    info.CRTVersion = nil
    lu.assertEquals(support[plat], info)
end
