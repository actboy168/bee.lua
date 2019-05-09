local lm = require 'luamake'

lm.c = lm.plat == 'msvc' and 'c89' or 'c11'
lm.cxx = 'c++17'

LUAI_MAXCSTACK = 1000

if lm.plat == 'msvc' or lm.plat == 'mingw' then
    require 'project.luamake.windows'
else
    require(('project.luamake.%s'):format(lm.plat))
end
