local platform = require 'bee.platform'
local subprocess = require 'bee.subprocess'
local fs = require 'bee.filesystem'

local isWindows = platform.OS == 'Windows'
local isMingw = os.getenv 'MSYSTEM' ~= nil
local isWindowsShell = (isWindows) and (not isMingw)

local function runshell(command)
    local p <close> = assert(io.popen(command))
    return p:read "a"
end

local function isWSL2()
    if platform.OS ~= "Linux" then
        return false
    end
    local r = runshell "uname -r"
    return r:match "WSL2" ~= nil
end

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
    return runshell(command):gsub('[\n\r]*$', '')
end

function shell:path()
    if isWindowsShell then
        return 'cmd', '/c'
    else
        return '/bin/sh', '-c'
    end
end

local luaexe = (function()
    local i = 0
    while arg[i] ~= nil do
        i = i - 1
    end
    return arg[i + 1]
end)()
luaexe = fs.absolute(fs.path(luaexe)):string()

function shell:runlua(script, option)
    local init = ("package.cpath = [[%s]]"):format(package.cpath)
    option = option or {}
    option[1] = {
        luaexe,
        '-e', init.."\n"..script.."\nos.exit(true)",
    }
    return subprocess.spawn(option)
end

shell.isMingw = isMingw
shell.isWindowsShell = isWindowsShell
shell.isWSL2 = isWSL2()

return shell
