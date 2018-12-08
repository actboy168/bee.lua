local fs = require "bee.filesystem"

local function foreach_dir(dir, callback)
    for path in dir:list_directory() do
        if fs.is_directory(path) then
            foreach_dir(path, callback)
        else
            callback(path)
        end
    end
end

local function readfile(path)
    local f = assert(io.open(path:string(), "rb"))
    local str = f:read "a"
    f:close()
    return str
end

local function writefile(path, str)
    local f = assert(io.open(path:string(), "wb"))
    f:write(str)
    f:close()
end

local res = {}

local function cpp_to_o(path)
    return path:gsub("\\", "_"):gsub("%.cpp", "")
end

local function gen_cpp(path, deps, sysdeps)
    path = path:string()
    res[#res + 1] = ("$(TMPDIR)/%s.o : %s %s | $(TMPDIR)"):format(cpp_to_o(path), path, table.concat(deps, " "))
    local inc = ""
    if sysdeps["lua.hpp"] then
        inc = inc .. " -I$(LUADIR)"
    end
    if sysdeps["lua-seri.h"] then
        inc = inc .. " -I$(LUASERIDIR)"
    end
    res[#res + 1] = ("\t$(CXX) -c $(CFLAGS) -o $@ $< %s -I."):format(inc)
    res[#res + 1] = ""
end

local function gen_dir(dir, deps)
    foreach_dir(dir, function(path)
        if path:extension():string() ~= ".cpp" then
            return
        end
        local source = readfile(path)
        local deps = {}
        local sysdeps = {}
        for name in source:gmatch '#[%s]*include[%s]*<([a-z%_%-%/]+%.hp?p?)>' do
            if name:sub(1, 4) == 'bee/' then
                deps[#deps + 1] = name
            elseif name:sub(1, 3) == 'lua' then
                sysdeps[name] = true
            end
        end
        gen_cpp(path, deps, sysdeps)
    end)
end

res[#res + 1] = "$(TMPDIR)/lua-seri.o : $(LUASERIDIR)/lua-seri.c $(LUASERIDIR)/lua-seri.h | $(TMPDIR)"
res[#res + 1] = "\t$(CC) -c $(CFLAGS) -o $@ $< -I$(LUADIR) "
res[#res + 1] = ""

gen_dir(fs.path "bee")
gen_dir(fs.path "binding")

local output = ...
writefile(fs.path(output), table.concat(res, "\n"))
print "success"