---@meta bee.subprocess

---子进程库
---@class bee.subprocess
local subprocess = {}

---进程对象
---@class bee.subprocess.process
---@field stdin file*? 标准输入文件句柄（如果请求了的话）
---@field stdout file*? 标准输出文件句柄（如果请求了的话）
---@field stderr file*? 标准错误文件句柄（如果请求了的话）
local process = {}

---等待进程结束
---@return integer? # 进程退出码，失败返回nil
---@return string? # 错误消息
function process:wait()
end

---向进程发送信号
---@param signum? integer 信号编号，默认为SIGTERM(15)
---@return boolean # 是否成功发送
function process:kill(signum)
end

---获取进程ID
---@return integer # 进程ID
function process:get_id()
end

---检查进程是否正在运行
---@return boolean
function process:is_running()
end

---恢复挂起的进程
---@return boolean # 是否成功
function process:resume()
end

---获取进程的原生句柄
---@return lightuserdata # 进程句柄
function process:native_handle()
end

---分离进程
---分离后进程不再由当前对象管理
---@return boolean # 是否成功
function process:detach()
end

---启动配置
---@class bee.subprocess.spawn_args
---@field [1] string|bee.fspath 程序路径
---@field [integer] string|bee.fspath|bee.subprocess.spawn_args 命令行参数（可嵌套数组）
---@field cwd? string|bee.fspath 工作目录
---@field stdin? boolean|file* true则创建管道，也可传入文件句柄
---@field stdout? boolean|file* true则创建管道，也可传入文件句柄
---@field stderr? boolean|file*|"stdout" true则创建管道，"stdout"则共享stdout
---@field env? table<string, string|false> 环境变量，false表示删除该变量
---@field suspended? boolean 是否以挂起状态启动
---@field detached? boolean 是否以分离模式启动
---@field console? "new"|"disable"|"inherit"|"detached" Windows控制台模式
---@field hideWindow? boolean Windows是否隐藏窗口
---@field searchPath? boolean Windows是否搜索PATH

---启动子进程
---@param args bee.subprocess.spawn_args 启动配置
---@return bee.subprocess.process? # 进程对象
---@return string? # 错误消息
function subprocess.spawn(args)
end

---等待多个进程中的任意一个
---@param processes bee.subprocess.process[] 进程数组
---@param timeout? integer 超时时间，单位为毫秒，-1表示无限等待
---@return boolean? # 成功返回true，失败返回nil
---@return string? # 错误消息
function subprocess.select(processes, timeout)
end

---检查管道中是否有可读数据
---@param file file* 文件句柄
---@return integer? # 可读字节数
---@return string? # 错误消息
function subprocess.peek(file)
end

---设置环境变量
---@param name string 环境变量名
---@param value string 环境变量值
---@return boolean? # 成功返回true，失败返回nil
---@return string? # 错误消息
function subprocess.setenv(name, value)
end

---获取当前进程ID
---@return integer # 进程ID
function subprocess.get_id()
end

---对参数进行命令行引用处理
---处理空格、引号等特殊字符
---@param arg string 原始参数
---@return string # 处理后的参数
function subprocess.quotearg(arg)
end

return subprocess
