local path = ...
local msys2 = require "msys2"
msys2:initialize(path)
msys2:exec("gcc --version")
msys2:exec("./configure")
msys2:exec("make")
