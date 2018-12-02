local sp = require 'bee.subprocess'
local fs = require 'bee.filesystem'
local thd = require 'bee.thread'

local function fork(script, args, options)
    local init = {
        fs.exe_path(),
        '-E',
        '-e', ('package.path=[[%s]]'):format(package.path),
        '-e', ('package.cpath=[[%s]]'):format(package.cpath),
        script,
        args,
        cwd = fs.current_path(),
    }
    if options then
        for k, v in pairs(options) do
            if type(k) == 'string' then
                init[k] = v
            end
        end
    end
    return sp.swapn(init)
end

local function thread(script, cfunction)
    return thd.thread(([=[
--%s
package.path=[[%s]]
package.cpath=[[%s]]
require %q]=]):format(
        script, package.path, package.cpath, script
    ), cfunction)
end

return {
    fork = fork,
    thread = thread,
}
