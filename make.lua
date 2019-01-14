local platform = require 'bee.platform'
local lm = require 'luamake'

local plat = (function ()
    if platform.OS == "Windows" then
        if os.getenv "MSYSTEM" then
            return "mingw"
        end
        return "msvc"
    elseif platform.OS == "Linux" then
        return "linux"
    elseif platform.OS == "macOS" then
        return "macos"
    end
end)()

local outdir = plat
if plat == "msvc" then
    local arguments = {}
    local arg = table.pack(...)
    local i = 1
    while i <= #arg do
        if arg[i]:sub(1, 1) == '-' then
            local k = arg[i]:sub(2)
            i = i + 1
            arguments[k] = arg[i]
        end
        i = i + 1
    end
    local arch = arguments.arch or 'x86'
    outdir = outdir .. '_' .. arch
end
outdir = outdir .. '_release'

lm.bindir = 'bin/' .. outdir
lm.objdir = 'tmp/' .. outdir

require(('project.luamake.%s'):format(platform.OS:lower()))
