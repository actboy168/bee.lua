local lm = require 'luamake'

lm.c = lm.plat == 'msvc' and 'c89' or 'c11'
lm.cxx = 'c++17'

require(('project.luamake.%s'):format(lm.plat))
