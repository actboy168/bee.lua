---@meta bee.platform

---平台信息（模块返回的是表而不是类）
---@class bee.platform
---@field os string 操作系统名称: "windows"|"android"|"linux"|"netbsd"|"freebsd"|"openbsd"|"ios"|"macos"|"unknown"
---@field Compiler string 编译器类型: "clang"|"msvc"|"gcc"|"unknown"
---@field CompilerVersion string 编译器版本字符串
---@field CRT string C运行时库类型: "bionic"|"msvc"|"libstdc++"|"libc++"|"unknown"
---@field CRTVersion string C运行时库版本字符串
---@field Arch string 架构: "arm64"|"x86"|"x86_64"|"arm"|"riscv"|"wasm32"|"wasm64"|"mips64el"|"loongarch64"|"ppc64"|"ppc"|"unknown"
---@field DEBUG boolean 是否为调试构建
---@field os_version bee.platform.os_version 操作系统版本信息

---操作系统版本信息
---@class bee.platform.os_version
---@field major integer 主版本号
---@field minor integer 次版本号
---@field revision integer 修订版本号

return {}
