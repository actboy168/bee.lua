local fs = require 'bee.filesystem'

local msvc = require 'msvc'
if not msvc:initialize(141, 'utf8') then
    error('Cannot found Visual Studio Toolset.')
end

local root = fs.absolute(fs.path '../')
local property = {
    Configuration = 'Debug',
    Platform = 'x86'
}
msvc:compile('build', root / 'project' / 'bee.sln', property)
