---@meta bee.debugging

---调试库
---@class bee.debugging
local debugging = {}

---触发断点
---在调试器中会中断执行
function debugging.breakpoint()
end

---检查是否有调试器附加
---@return boolean 是否有调试器附加
function debugging.is_debugger_present()
end

---仅在有调试器附加时触发断点
---如果没有调试器附加，则不执行任何操作
function debugging.breakpoint_if_debugging()
end

return debugging
