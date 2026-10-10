-- 通用 Lua 补丁基础设施：构建期把官方源码整树复制到 $builddir/patched/lua<ver>/，
-- 再按注册表顺序 git apply 启用的补丁；整树复制使 include 始终解析到补丁目录内
-- 的文件，无补丁时退化为纯复制。
--
-- 这是生成 ninja 规则的构建期模块（require 进来调用），不是构建期脚本。
-- 用法：require("compile.apply_patch").setup(srcdir, patches)
-- patches = { { flag = "optchain", dir = "optchain" }, ... }，补丁文件约定为
-- 3rd/lua-patch/<dir>/lua<ver>.patch，flag 省略则始终启用；setup 设置
-- lm.luadir 并生成 apply_lua_patch。

local lm = require "luamake"
local fs = require "bee.filesystem"

local M = {}

-- 命令里手拼的路径要归一化到外层工程根（inputs/outputs 由 luamake 处理）
local function rel(path)
    return tostring(lm:path(path))
end

-- cmd 内建命令会把路径里的 / 当开关前缀，必须换反斜杠；$builddir 的值
-- 生成期拿不到，故 cmd 分支用字面量 builddir
local function nt(path)
    return (tostring(path):gsub("%$builddir", lm.builddir):gsub("/", "\\"))
end

-- 解析补丁涉及的文件：modified/deleted 都要先还原为原样（否则重复应用会失败，
-- 且删掉文件前它得存在），其中 deleted 不作为输出；created 由 git apply 创建。
local function patch_targets(patchfile)
    local targets = {}
    local path, kind
    for rawline in io.lines((fs.path(lm.workdir) / patchfile):string()) do
        local line = rawline:gsub("\r$", "")
        local _, b = line:match("^diff %-%-git a/(.*) b/(.*)$")
        if b then
            path, kind = b, "modified"
        elseif line:match("^new file mode") or line:match("^%-%-%- /dev/null") then
            kind = "created"
        elseif line:match("^deleted file mode") or line:match("^%+%+%+ /dev/null") then
            kind = "deleted"
        elseif line:match("^%+%+%+ ") and path then
            targets[path] = kind
            path = nil
        end
    end
    return targets
end

function M.setup(srcdir, patches)
    lm.luadir = lm:path("$builddir/patched/"..srcdir:match("[^/]+$"))
    local dstdir = tostring(lm.luadir)

    local patchfiles = {}
    for _, p in ipairs(patches) do
        if not p.flag or lm[p.flag] then
            local patchfile = ("3rd/lua-patch/%s/lua%s.patch"):format(p.dir, lm.lua)
            assert(fs.exists(fs.path(lm.workdir) / patchfile), "patch not found: " .. patchfile)
            patchfiles[#patchfiles+1] = patchfile
        end
    end

    local kind_of = {}
    for _, patchfile in ipairs(patchfiles) do
        for name, kind in pairs(patch_targets(patchfile)) do
            kind_of[name] = kind_of[name] or kind
        end
    end

    -- 整树清单，排序保证生成结果稳定
    local names = {}
    local srcpath = fs.path(lm.workdir) / srcdir
    for file in fs.pairs_r(srcpath) do
        if fs.is_regular_file(file) then
            names[#names+1] = fs.relative(file, srcpath):string()
        end
    end
    table.sort(names)

    local restore = {}   -- 打补丁前需要还原为原样的文件（modified + deleted）
    local outputs = {}   -- 补丁规则的输出（modified）
    for _, name in ipairs(names) do
        local kind = kind_of[name]
        if kind == "deleted" then
            restore[#restore+1] = name
        elseif kind == "modified" then
            restore[#restore+1] = name
            outputs[#outputs+1] = name
        end
    end
    local created = {}
    for name, kind in pairs(kind_of) do
        if kind == "created" then
            created[#created+1] = name
        end
    end
    table.sort(created)

    -- 未被补丁触及的文件：逐文件 copy，逐文件边只在该源文件变化时重写
    local copy_inputs, copy_outputs = {}, {}
    for _, name in ipairs(names) do
        if not kind_of[name] then
            copy_inputs[#copy_inputs+1] = srcdir.."/"..name
            copy_outputs[#copy_outputs+1] = dstdir.."/"..name
        end
    end
    if #copy_inputs > 0 then
        lm:copy "lua_src" {
            inputs = copy_inputs,
            outputs = copy_outputs,
        }
    end

    -- 打补丁：先把涉及的文件还原为原样，再按序 git apply。不能用 lm:runlua ——
    -- runlua 在 prebuilt 模式下会把 bootstrap 作为隐式输入，而 bootstrap 要用打
    -- 补丁后的头文件编译，两者互为前置成环；这里只依赖 sh/cmd 与 git，环被切断。
    if #patchfiles > 0 then
        local batch = lm.hostshell == "cmd"
        local srcrel = rel(srcdir)
        local args

        if batch then
            -- cmd 分支：拆成参数列表拼出命令行（copy/del 与 && 都要 cmd 解析）
            local srcnt, dstnt = nt(srcrel), nt(dstdir)
            args = { "cmd", "/c" }
            local function add(...)
                if #args > 2 then
                    args[#args+1] = "&&"
                end
                for _, v in ipairs({...}) do
                    args[#args+1] = v
                end
            end
            for _, name in ipairs(restore) do
                add("copy", "/y", srcnt.."\\"..name, dstnt.."\\"..name)
            end
            for _, name in ipairs(created) do
                -- 先删掉要新建的文件（git apply 拒绝覆盖已存在的目标）；del 对不
                -- 存在的文件会失败并中断 && 链，故套一层 cmd 保证返回 0。
                add("cmd", "/c", ('"del /q %s\\%s 2>nul || exit /b 0"'):format(dstnt, name))
            end
            for _, patchfile in ipairs(patchfiles) do
                add("git", "apply", "--directory="..dstdir, rel(patchfile))
            end
        else
            -- 其余平台（含 windows 上的 sh）：整条命令交给 sh
            local parts = {}
            if #restore > 0 then
                local srcs = {}
                for _, name in ipairs(restore) do
                    srcs[#srcs+1] = srcrel.."/"..name
                end
                parts[#parts+1] = ("cp -f %s %s/"):format(table.concat(srcs, " "), dstdir)
            end
            for _, name in ipairs(created) do
                parts[#parts+1] = ("rm -f %s/%s"):format(dstdir, name)
            end
            for _, patchfile in ipairs(patchfiles) do
                parts[#parts+1] = ("git apply --directory=%s %s"):format(dstdir, rel(patchfile))
            end
            local command = table.concat(parts, " && ")
            if lm.hostos == "windows" then
                -- windows 上经 cmd 执行命令，需显式加引号：cmd 会解释 &&，
                -- 且 quotearg 不会加引号（sh -c 否则只吃到第一个词）
                args = { "sh", "-c", '"'..command..'"' }
            else
                -- 非 windows 平台 ninja 自己用 sh -c 执行，整条命令作为单个参数
                args = { command }
            end
        end

        local patch_inputs, patch_outputs = {}, {}
        for _, name in ipairs(restore) do
            patch_inputs[#patch_inputs+1] = srcdir.."/"..name
        end
        for _, patchfile in ipairs(patchfiles) do
            patch_inputs[#patch_inputs+1] = patchfile
        end
        for _, name in ipairs(outputs) do
            patch_outputs[#patch_outputs+1] = dstdir.."/"..name
        end
        for _, name in ipairs(created) do
            patch_outputs[#patch_outputs+1] = dstdir.."/"..name
        end

        lm:rule "lua_patch" {
            args = args,
            description = "Patch   lua"..lm.lua,
            restat = true,
        }
        lm:build "lua_patch_apply" {
            rule = "lua_patch",
            inputs = patch_inputs,
            outputs = patch_outputs,
        }
    end

    -- 编译前必须保证补丁目录整树就绪：头文件依赖是编译期才由 depfile 发现的，
    -- 首次构建时 ninja 并不知道它们由谁产生。
    local apply_deps = {}
    if #copy_inputs > 0 then
        apply_deps[#apply_deps+1] = "lua_src"
    end
    if #patchfiles > 0 then
        apply_deps[#apply_deps+1] = "lua_patch_apply"
    end
    lm:phony "apply_lua_patch" {
        deps = apply_deps,
    }
end

return M
