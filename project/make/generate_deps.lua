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
local alldeps = {}

local function cpp_to_o(path)
    return path:gsub("\\", "_"):gsub("%.cpp", "")
end

local function deps_dir(dir)
    foreach_dir(dir, function(path)
        local ext = path:extension():string()
        if ext ~= ".cpp" and ext ~= ".h" then
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
        alldeps[path:string():gsub("\\", "/")] = {deps, sysdeps}
    end)
end

local function calc_deps_file(p, a, b)
    if a[p] or not alldeps[p] then
        return
    end
    a[#a+1] = p
    a[p] = true
    for _, v in ipairs(alldeps[p][1]) do
        calc_deps_file(v, a, b)
    end
    for v in pairs(alldeps[p][2]) do
        b[v] = true
    end
end

local function calc_deps()
    local new = {}
    for p, d in pairs(alldeps) do
        local a = {}
        local b = {}
        calc_deps_file(p, a, b)
        new[p] = {a, b}
    end
    alldeps = new
end

local function gen_file(path)
    path = path:string()
    local deps, sysdeps = alldeps[path:gsub("\\", "/")][1], alldeps[path:gsub("\\", "/")][2]
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

local function gen_dir(dir)
    foreach_dir(dir, function(path)
        if path:extension():string() ~= ".cpp" then
            return
        end
        gen_file(path)
    end)
end

res[#res + 1] = "$(TMPDIR)/lua-seri.o : $(LUASERIDIR)/lua-seri.c $(LUASERIDIR)/lua-seri.h | $(TMPDIR)"
res[#res + 1] = "\t$(CC) -c $(CFLAGS) -o $@ $< -I$(LUADIR) "
res[#res + 1] = ""

deps_dir(fs.path "bee")
deps_dir(fs.path "binding")
calc_deps()

gen_dir(fs.path "bee")
gen_dir(fs.path "binding")

local output = ...
writefile(fs.path(output), table.concat(res, "\n"))
print "success"