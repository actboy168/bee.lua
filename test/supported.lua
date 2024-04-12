local platform = require "bee.platform"

local m = {}

local feature = {}

function feature.symlink()
    if platform.os ~= "windows" then
        return true
    end
    -- see https://blogs.windows.com/windowsdeveloper/2016/12/02/symlinks-windows-10/
    local fs = require "bee.filesystem"
    local ok = pcall(fs.create_symlink, "temp.txt", "temp.link")
    fs.remove_all "temp.link"
    return ok
end

function feature.hardlink()
    return platform.os ~= "android"
end

local mt = {}

function mt:__index(what)
    local initfunc = feature[what]
    if initfunc == nil then
        error("undefined feature:"..what)
    end
    local res = initfunc()
    m[what] = res
    return res
end

function mt:__call(what)
    return self[what]
end

setmetatable(m, mt)
return m
