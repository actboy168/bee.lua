local helper = require 'msvc_helper'
local prefix = helper.get_prefix(helper.get_env(helper.get_path()))
local fs = require 'bee.filesystem'

local cwd = fs.exe_path():parent_path()
local output = cwd / 'build' / 'msvc' / 'msvc_deps_prefix.ninja'

assert(
    assert(
        io.open(output:string(), 'wb')
    ):write(("deps_prefix = %s"):format(prefix))
):close()
