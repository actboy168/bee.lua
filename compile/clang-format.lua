local fs = require "bee.filesystem"
local sp = require "bee.subprocess"

local EXTENSION <const> = {
    [".h"] = true,
    [".c"] = true,
    [".cpp"] = true,
}

local sourcefile = {}

local function scan(dir)
    for path, status in fs.pairs(dir) do
        if status:is_directory() then
            scan(path)
        else
            local ext = path:extension()
            if EXTENSION[ext] then
                sourcefile[#sourcefile+1] = path:string()
            end
        end
    end
end

scan "bee"
scan "binding"
scan "bootstrap"

if #sourcefile > 0 then
    local process = assert(sp.spawn {
        "luamake", "shell", "clang-format",
        "-i", sourcefile,
        stdout = io.stdout,
        stderr = "stdout",
        searchPath = true,
    })
    local code = process:wait()
    if code ~= 0 then
        os.exit(code, true)
    end
end
