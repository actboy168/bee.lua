-- 将源文件复制到构建目录，并应用 git 补丁。
-- 用法: apply_patch.lua <src_dir> <dst_dir> <patch_file> [file...]
--
-- 把 file... 指定的文件从 src_dir 复制到 dst_dir（父目录不存在则创建），
-- 然后在 dst_dir 上应用补丁。每次都从 src_dir 重新复制，因此脚本幂等，
-- 无需清理 dst_dir；内容没有变化的文件不会被重写（保持 mtime 稳定，
-- 配合构建规则的 restat 支持增量编译）。
local fs = require "bee.filesystem"

local src, dst, patch = ...
assert(src and dst and patch, "usage: apply_patch.lua <src_dir> <dst_dir> <patch_file> [file...]")

local function read_file(path)
    local f <close> = io.open(path, "rb")
    if not f then
        return nil
    end
    return f:read "a"
end

fs.create_directories(dst)
for i = 4, select("#", ...) do
    local file = select(i, ...)
    local new = assert(read_file(src .. "/" .. file), "file not found: " .. file)
    if read_file(dst .. "/" .. file) ~= new then
        local parent = file:match "^(.*)/[^/]+$"
        if parent then
            fs.create_directories(dst .. "/" .. parent)
        end
        local f <close> = assert(io.open(dst .. "/" .. file, "wb"))
        f:write(new)
    end
end

local ok = os.execute(('git apply --directory="%s" "%s"'):format(dst, patch))
assert(ok, "git apply failed: " .. patch)
