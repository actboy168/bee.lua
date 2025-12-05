---@meta bee.platform

---平台信息（模块返回的是表而不是类）
---@class bee.platform
---@field os "windows"|"android"|"linux"|"netbsd"|"freebsd"|"openbsd"|"ios"|"macos"|"unknown" 操作系统名称
---@field Compiler "clang"|"msvc"|"gcc"|"unknown" 编译器类型
---@field CompilerVersion string 编译器版本字符串
---@field CRT "bionic"|"msvc"|"libstdc++"|"libc++"|"unknown" C运行时库类型
---@field CRTVersion string C运行时库版本字符串
---@field Arch "arm64"|"x86"|"x86_64"|"arm"|"riscv"|"wasm32"|"wasm64"|"mips64el"|"loongarch64"|"ppc64"|"ppc"|"unknown" 架构
---@field DEBUG boolean 是否为调试构建
---@field os_version bee.platform.os_version 操作系统版本信息
local platform = {}

---操作系统版本信息
---@class bee.platform.os_version
---@field major integer 主版本号
---@field minor integer 次版本号
---@field revision integer 修订版本号

return platform
