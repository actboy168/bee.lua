local fs = require 'bee.filesystem'
local sp = require 'bee.subprocess'

local function convertpath(s)
    return s:gsub("\\", "/"):gsub("([a-zA-z]):", "/%1")
end

local mt = {}

function mt:initialize(path)
    self.path = fs.path(path)
    self.bash = self.path / "usr" / "bin" / "bash.exe"
end

function mt:exec(cmd, cwd)
    cwd = cwd or fs.current_path():string()
    local command = ([[export PATH=$PATH:/mingw64/bin:/usr/local/bin:/usr/bin:/bin:/opt/bin; cd %s; %s]]):format(
        convertpath(cwd),
        convertpath(cmd)
    )
    local process = assert(sp.spawn {
        self.bash,
        "-lc",
        command,
        stderr = io.stdout,
        stdout = io.stderr,
    })
    local code = process:wait()
    if code ~= 0 then
        os.exit(code, true)
    end
end

return mt
