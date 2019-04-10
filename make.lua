local lm = require 'luamake'

local outdir = lm.plat
if lm.plat == "msvc" then
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

lm.c = lm.plat == 'msvc' and 'c89' or 'c11'
lm.cxx = 'c++17'

LUAI_MAXCSTACK = 1000

if lm.plat == 'msvc' or lm.plat == 'mingw' then
    require 'project.luamake.windows'
else
    require(('project.luamake.%s'):format(lm.plat))
end
