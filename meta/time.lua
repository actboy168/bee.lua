---@meta bee.time

---时间库
---@class bee.time
local time = {}

---获取当前系统时间
---返回自Unix纪元(1970-01-01 00:00:00 UTC)以来的毫秒数
---@return integer # 时间戳，单位为毫秒
function time.time()
end

---获取单调递增时间
---用于测量时间间隔，不受系统时间调整影响
---@return integer # 时间戳，单位为毫秒
function time.monotonic()
end

---获取当前线程的CPU时间
---@return integer # CPU时间，单位为毫秒
function time.thread()
end

return time
