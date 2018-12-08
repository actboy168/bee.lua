local path = ...
local f = assert(io.open("make/.mingw", "wb"))
f:write(path)
f:close()
