local platform = require "bee.platform"
local fs = require "bee.filesystem"
local subprocess = require "bee.subprocess"
local lt = require "ltest"

local isWindows <const> = platform.os == "windows"
local isWindowsShell <const> = isWindows and (os.getenv "MSYSTEM" == nil)

local function runshell(command)
    local p <close> = assert(io.popen(command))
    return p:read "a"
end

local shell = {}

function shell:add_readonly(filename)
    if isWindowsShell then
        os.execute(("attrib +r %q"):format(filename))
    else
        os.execute(("chmod a-w %q"):format(filename))
    end
end

function shell:del_readonly(filename)
    if isWindowsShell then
        os.execute(("attrib -r %q"):format(filename))
    else
        os.execute(("chmod a+w %q"):format(filename))
    end
end

function shell:pwd()
    local command = isWindows and "echo %cd%" or "pwd -P"
    return runshell(command):gsub("[\n\r]*$", "")
end

local luaexe <const> = (function ()
    local i = 0
    while arg[i] ~= nil do
        i = i - 1
    end
    local exe = arg[i + 1]
    exe = fs.absolute(fs.path(exe)):string()
    if isWindows and not exe:match "%.%w+$" then
        exe = exe..".exe"
    end
    return exe
end)()

local initscript <const> = (function ()
    local cpaths = {}
    for cpath in package.cpath:gmatch "[^;]*" do
        if cpath:sub(1, 1) == "." then
            cpaths[#cpaths+1] = fs.absolute(cpath):string()
        else
            cpaths[#cpaths+1] = cpath
        end
    end
    return ("package.cpath = [[%s]]"):format(table.concat(cpaths, ";"))
end)()

function shell:runlua(script, option)
    option = option or {}
    local filename = option[1]
    option[1] = {
        luaexe,
        "-e", initscript.."\n"..script.."\nos.exit(true)",
        filename
    }
    local process, errmsg = subprocess.spawn(option)
    lt.assertIsUserdata(process, errmsg)
    return process
end

return shell
