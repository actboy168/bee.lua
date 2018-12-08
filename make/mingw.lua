local function get_mingw_path()
    local f = assert(io.open("make/.mingw", "rb"))
    local str = f:read "a"
    f:close()
    return str
end
local cmd = table.concat(table.pack(...), " ")
local msys2 = require "msys2"
msys2:initialize(get_mingw_path())
msys2:exec(cmd)
