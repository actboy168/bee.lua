local support = {}

support.msbuild = {
    OS = 'Windows',
    Arch = 'x86',
    Compiler = 'msvc',
    CRT = 'msvc',
}
support.msvc = {
    OS = 'Windows',
    Arch = 'x86',
    Compiler = 'msvc',
    CRT = 'msvc',
}
support.mingw = {
    OS = 'Windows',
    Arch = 'x86_64',
    Compiler = 'gcc',
    CRT = 'libstdc++',
}
support.linux = {
    OS = 'Linux',
    Arch = 'x86_64',
    Compiler = 'clang',
    CRT = 'libstdc++',
}
support.macos = {
    OS = 'macOS',
    Arch = 'x86_64',
    Compiler = 'clang',
    CRT = 'libc++',
}

local lu = require 'ltest'
local fs = require 'bee.filesystem'
local platform = require 'bee.platform'

local test_plat = lu.test "platform"

function test_plat:test_1()
    lu.assertNotEquals(__Target__, nil)
    local plat = fs.path(__Target__):string():lower():match "build[/\\](%a+)[/\\]bin"
    if plat == 'linux' then
        lu.assertEquals(platform.Compiler == 'gcc' or platform.Compiler == 'clang', true)
        support.linux.Compiler = platform.Compiler
    end
    if plat == 'msvc' or plat == 'msbuild' then
        lu.assertEquals(platform.Arch == 'x86' or platform.Arch == 'x86_64', true)
        support[plat].Arch = platform.Arch
    end
    if plat == 'macos' then
        lu.assertEquals(platform.Arch == 'x86_64' or platform.Arch == 'arm64', true)
        support[plat].Arch = platform.Arch
    end
    local info = {}
    for k, v in pairs(platform) do
        info[k] = v
    end
    info.CompilerVersion = nil
    info.CRTVersion = nil
    info.DEBUG = nil
    lu.assertEquals(support[plat], info)
end
