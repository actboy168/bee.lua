-- 3rd/lua-patch/apply_patch.lua
-- 复制官方 Lua 源码目录到构建目录，并应用可选链补丁（git apply）。
-- 用法: apply_patch.lua <src_dir> <patch_file> <dst_dir>
local src, patch, dst = ...
assert(src and patch and dst, "usage: apply_patch.lua <src_dir> <patch_file> <dst_dir>")

local is_windows = package.config:sub(1, 1) == "\\"

-- 清空并重建目标目录
if is_windows then
    os.execute(('if exist "%s" rmdir /s /q "%s"'):format(dst, dst))
    os.execute(('mkdir "%s"'):format(dst))
else
    os.execute(('rm -rf "%s"'):format(dst))
    os.execute(('mkdir -p "%s"'):format(dst))
end

-- 复制官方源码目录（保持完整，编译时 include 自包含）
if is_windows then
    os.execute(('xcopy /e /i /y /q "%s" "%s"'):format(src, dst))
else
    os.execute(('cp -r "%s/." "%s/"'):format(src, dst))
end

-- 应用补丁（patch 内为相对路径，--directory 指定目标目录）
local dst_str = dst:gsub("\\", "/")
local ok = os.execute(('git apply --directory="%s" "%s"'):format(dst_str, patch))
assert(ok, "git apply failed for " .. patch)
print("apply_patch: OK")
