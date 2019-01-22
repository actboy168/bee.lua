local helper = require 'msvc_helper'
local prefix = helper.get_prefix(helper.get_env(helper.get_path()))

assert(
    assert(
        io.open('build/msvc/msvc_deps_prefix.ninja', 'wb')
    ):write(("deps_prefix = %s"):format(prefix))
):close()
