-- 将源码树整树复制到构建目录，并应用 git 补丁。
-- 用法: apply_patch.lua <src_dir> <dst_dir> [patch...]
--
-- 在暂存目录里组装最终内容（整树复制 + 按序打补丁），再与 dst 逐文件
-- 比对：只有内容变化的文件才重写（条件写，配合规则的 restat 使下游按
-- 内容精确重编，且 mtime 永不倒退）；不在暂存内容中的 dst 文件删除。
-- 不传补丁时退化为纯复制，不调用 git。
local fs = require "bee.filesystem"

local src, dst = ...
assert(src and dst, "usage: apply_patch.lua <src_dir> <dst_dir> [patch...]")

-- 暂存目录放在 dst 同级，避免被下面的过期文件清理误删
local stage = dst .. ".stage"
fs.remove_all(stage)
fs.create_directories(stage)
fs.create_directories(dst)

local function read_file(path)
    local f <close> = io.open(path, "rb")
    if not f then
        return nil
    end
    return f:read "a"
end

local function write_file(path, content)
    local parent = path:match "^(.*)/[^/]+$"
    if parent then
        fs.create_directories(parent)
    end
    local f <close> = assert(io.open(path, "wb"))
    f:write(content)
end

local function copy_tree(from, to)
    local frompath = fs.path(from)
    for file in fs.pairs_r(frompath) do
        if fs.is_regular_file(file) then
            local target = fs.path(to) / fs.relative(file, frompath)
            fs.create_directories(target:parent_path())
            fs.copy_file(file, target, fs.copy_options.overwrite_existing)
        end
    end
end

-- 1. 整树复制到暂存目录
copy_tree(src, stage)

-- 2. 按序应用补丁
for i = 3, select("#", ...) do
    local patch = select(i, ...)
    local ok = os.execute(('git apply --directory="%s" "%s"'):format(stage, patch))
    assert(ok, "git apply failed: " .. patch)
end

-- 3. 条件写同步到 dst；keep 集即暂存内容（含补丁新建的文件）
local keep = {}
local stagepath = fs.path(stage)
for file in fs.pairs_r(stagepath) do
    local rel = fs.relative(file, stagepath)
    if fs.is_regular_file(file) then
        keep[rel:string()] = true
        local new = assert(read_file(file:string()), "read failed: " .. file:string())
        local target = (fs.path(dst) / rel):string()
        if read_file(target) ~= new then
            write_file(target, new)
        end
    end
end

-- 4. 删除不在暂存内容中的过期文件
local dstpath = fs.path(dst)
local dirs = {}
for file in fs.pairs_r(dstpath) do
    if fs.is_regular_file(file) then
        if not keep[fs.relative(file, dstpath):string()] then
            fs.remove(file)
        end
    else
        dirs[#dirs+1] = file
    end
end

-- 5. 按深度倒序删除空目录
table.sort(dirs, function (a, b) return #a:string() > #b:string() end)
for _, dir in ipairs(dirs) do
    if fs.pairs(dir)() == nil then
        fs.remove(dir)
    end
end

fs.remove_all(stage)
