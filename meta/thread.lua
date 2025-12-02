---@meta bee.thread

---线程库
---@class bee.thread
---@field id integer 当前线程的ID（主线程为0）
local thread = {}

---创建一个新的线程
---线程中可以访问 bee.* 模块，但不共享全局变量
---@param source string Lua代码字符串，作为线程的入口点
---@param ... any 传递给线程的参数，会被序列化
---@return lightuserdata handle # 线程句柄
function thread.create(source, ...)
end

---获取线程错误日志
---如果有线程发生错误，返回错误消息
---@return string? # 错误消息，如果没有错误则返回nil
function thread.errlog()
end

---使当前线程休眠
---@param msec integer 休眠时间，单位为毫秒
function thread.sleep(msec)
end

---等待线程结束
---@param handle lightuserdata 线程句柄
function thread.wait(handle)
end

---设置当前线程的名称
---主要用于调试目的
---@param name string 线程名称
function thread.setname(name)
end

---预加载模块
---在新线程中调用以注册所有 bee.* 模块
---@param L lightuserdata Lua状态机指针
function thread.preload_module(L)
end

return thread
