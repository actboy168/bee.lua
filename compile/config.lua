local lm = require 'luamake'

local isWindows = lm.os == 'windows'

lm.cxx = 'c++17'

local plat = (function ()
    if isWindows then
        if lm.compiler == "gcc" then
            return "mingw"
        end
        return "msvc"
    end
    return lm.os
end)()
lm.builddir = ("build/%s"):format(plat)

lm.warnings = "error"

lm.windows = {
    defines = "_WIN32_WINNT=0x0601",
}
if lm.mode == "debug" and lm.arch == "x86_64" and lm.compiler == "msvc" then
    lm.windows.ldflags = "/STACK:"..0x160000
end

lm.macos = {
    flags = "-Wunguarded-availability",
}

if lm.os == "macos" then
    local function get_version()
        local f <close> = io.popen("sw_vers", "r")
        if f then
            for line in f:lines() do
                local major, minor = line:match "ProductVersion:%s*(%d+)%.(%d+)"
                if major and minor then
                    return tonumber(major) * 100 + tonumber(minor)
                end
            end
        end
    end
    local ver = get_version()
    if ver and ver >= 1015 then
        lm.macos.sys = "macos10.15"
    end
end

lm.linux = {
    flags = "-fPIC",
}
