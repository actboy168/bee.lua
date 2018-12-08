local path, cmd = ...
local msys2 = require "msys2"
msys2:initialize(path)
if cmd then
    msys2:exec(cmd)
end
msys2:exec("gcc --version")
msys2:exec("./configure")
msys2:exec("make")
