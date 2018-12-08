local fs = require "bee.filesystem"
local function get_mingw_path(cwd)
    local f = assert(io.open((cwd / ".mingw"):string(), "rb"))
    local str = f:read "a"
    f:close()
    return str
end
local cwd = fs.exe_path():parent_path()
local cmd = table.concat(table.pack(...), " ")
local msys2 = require "msys2"
msys2:initialize(get_mingw_path(cwd))
msys2:exec(cmd, cwd:parent_path():string())
