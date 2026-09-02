-- 将源文件复制到构建目录，并应用 git 补丁。
-- 用法: apply_patch.lua <src_dir> <dst_dir> <patch_file> [file...]
--
-- 把 file... 指定的文件从 src_dir 复制到 dst_dir（父目录不存在则创建），
-- 然后在 dst_dir 上应用补丁。每次都从 src_dir 重新复制，因此脚本幂等，
-- 无需清理 dst_dir。
--
-- 增量编译支持（配合构建规则的 restat）：
--   * 内容没有变化的文件不会被重写；
--   * 补丁后内容仍与之前一致的文件，恢复原来的修改时间，
--     避免下游以为产物变化而重新编译。
local src, dst, patch = ...

local fs_ok, fs = pcall(require, "bee.filesystem")

local function normalize(p)
    return (p:gsub("\\", "/"))
end

local is_windows = package.config:sub(1, 1) == "\\"

local function ensure_dir(dir)
    if is_windows then
        os.execute(('if not exist "%s" mkdir "%s"'):format(dir, dir))
    else
        os.execute(('mkdir -p "%s"'):format(dir))
    end
end

local function read_file(path)
    local f = io.open(path, "rb")
    if not f then
        return nil
    end
    local content = f:read "a"
    f:close()
    return content
end

src, dst, patch = normalize(src), normalize(dst), normalize(patch)

local files = {}
for i = 4, select("#", ...) do
    files[#files+1] = normalize(select(i, ...))
end

-- 记录补丁前的内容与修改时间
local snapshot = {}
if fs_ok then
    for _, file in ipairs(files) do
        local path = dst .. "/" .. file
        snapshot[file] = {
            read_file(path),
            fs.exists(fs.path(path)) and fs.last_write_time(fs.path(path)) or nil,
        }
    end
end

ensure_dir(dst)
for _, file in ipairs(files) do
    local parent = file:match "^(.*)/[^/]+$"
    if parent then
        ensure_dir(dst .. "/" .. parent)
    end
    local new = assert(read_file(src .. "/" .. file))
    local old = snapshot[file] and snapshot[file][1] or read_file(dst .. "/" .. file)
    if old ~= new then
        local f <close> = assert(io.open(dst .. "/" .. file, "wb"))
        f:write(new)
    end
end

local ok = os.execute(('git apply --directory="%s" "%s"'):format(dst, patch))
assert(ok, "git apply failed: " .. patch)

-- 补丁后内容没变的文件恢复修改时间
if fs_ok then
    for _, file in ipairs(files) do
        local state = snapshot[file]
        if state and state[2] and read_file(dst .. "/" .. file) == state[1] then
            fs.last_write_time(fs.path(dst) / fs.path(file), state[2])
        end
    end
end
