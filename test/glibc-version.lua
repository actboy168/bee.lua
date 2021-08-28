local function split(str)
    local r = {}
    str:gsub('[^%.]+', function (w) r[#r+1] = w end)
    return r
end

local function versionInt(str)
    local t = split(str)
    return t[1] * 10000 + t[2] * 100 + (t[3] or 0)
end

local function glibVersion(paths)
    local maxVersion = 0
    local symbols = {}
    local function search(path)
        local f <close>, err = io.popen(("objdump -T %s | grep GLIBC_"):format(path), "r")
        if not f then
            return nil, err
        end
        for line in f:lines() do
            local version, symbol = line:match "GLIBC_([%d%.]+)%s+([^%s]+)"
            if version and symbol then
                local ver = versionInt(version)
                if ver > maxVersion then
                    maxVersion = ver
                    symbols = { version, [symbol] = true }
                elseif ver == maxVersion then
                    symbols[symbol] = true
                end
            end
        end
    end
    for _, path in ipairs(paths) do
        search(path)
    end
    local s = {}
    for k in pairs(symbols) do
        if k ~= 1 then
            s[#s+1] = k
        end
    end
    table.sort(s)
    return string.format("Requirements GLIBC version %s or later. (%s)", symbols[1], table.concat(s, " "))
end

print(glibVersion {
    "./build/linux/bin/lua",
    "./build/linux/bin/bee.so",
})
