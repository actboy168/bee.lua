local platform = require 'bee.platform'

local isWindows = platform.OS == 'Windows'
local isMingw = os.getenv 'MSYSTEM' ~= nil
local isWindowsShell = (isWindows) and (not isMingw)

local shell = {}

function shell:add_readonly(filename)
    if isWindowsShell then
        os.execute(('attrib +r %q'):format(filename))
    else
        os.execute(('chmod a-w %q'):format(filename))
    end
end
function shell:del_readonly(filename)
    if isWindowsShell then
        os.execute(('attrib -r %q'):format(filename))
    else
        os.execute(('chmod a+w %q'):format(filename))
    end
end

function shell:pwd()
    local command = isWindows and 'echo %cd%' or 'pwd -P'
    return (io.popen(command):read 'a'):gsub('[\n\r]*$', '')
end

function shell:path()
    if isWindowsShell then
        return 'cmd', '/c'
    else
        return '/bin/sh', '-c'
    end
end

shell.isMingw = isMingw
shell.isWindowsShell = isWindowsShell

return shell
