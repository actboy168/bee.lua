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
    local process, stdout, stderr = sp.spawn {
        self.bash,
        "-lc",
        command,
        stderr = true,
        stdout = true,
    }
    if not process then
        error(stdout)
    end
    sp.filemode(stdout, 'b')
    sp.filemode(stderr, 'b')
    stdout:setvbuf 'no'
    stderr:setvbuf 'no'
    io.stdout:setvbuf 'no'
    io.stderr:setvbuf 'no'
    for l in stdout:lines() do
        io.stdout:write(l .. '\n')
    end
    stdout:close()
    for l in stderr:lines() do
        io.stderr:write(l .. '\n')
    end
    stderr:close()
    local code = process:wait()
    if code ~= 0 then
        os.exit(code, true)
    end
end

return mt
