local path = ...
local fs = require "bee.filesystem"
local cwd = fs.exe_path():parent_path()
local f = assert(io.open((cwd / ".mingw"):string(), "wb"))
f:write(path)
f:close()
