local helper = require 'msvc_helper'
local prefix = helper.get_prefix(helper.get_env(helper.get_path()))
local fs = require 'bee.filesystem'

local cwd = fs.exe_path():parent_path():parent_path()
local outdir = cwd / 'build' / 'msvc'
local output = outdir / 'msvc_deps_prefix.ninja'
fs.create_directories(outdir)

assert(
    assert(
        io.open(output:string(), 'wb')
    ):write(("deps_prefix = %s\n"):format(prefix))
):close()
