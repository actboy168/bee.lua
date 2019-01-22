local helper = require 'msvc_helper'
local prefix = helper.get_prefix(helper.get_env(helper.get_path()))
local fs = require 'bee.filesystem'

local cwd = fs.exe_path():parent_path():parent_path()
local outdir = cwd / 'build' / 'msvc'
local output = outdir / 'msvc-init.ninja'
fs.create_directories(outdir)

local template = [[
builddir = build/msvc
msvc_deps_prefix = %s
subninja ninja/msvc.ninja
]]

assert(
    assert(
        io.open(output:string(), 'wb')
    ):write(template:format(prefix))
):close()
