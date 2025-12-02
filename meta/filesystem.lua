---@meta bee.filesystem

---文件系统操作库
---@class bee.filesystem
local fs = {}

---文件路径对象
---@class bee.fspath
---@operator div(bee.fspath|string): bee.fspath 路径拼接 (使用 / 分隔)
---@operator concat(bee.fspath|string): bee.fspath 路径拼接 (直接拼接)
local path = {}

---获取路径的字符串表示
---@return string
function path:string()
end

---获取路径的文件名部分
---@return bee.fspath
function path:filename()
end

---获取路径的父目录部分
---@return bee.fspath
function path:parent_path()
end

---获取路径的主干部分(不含扩展名)
---@return bee.fspath
function path:stem()
end

---获取路径的扩展名
---@return string
function path:extension()
end

---判断路径是否为绝对路径
---@return boolean
function path:is_absolute()
end

---判断路径是否为相对路径
---@return boolean
function path:is_relative()
end

---移除路径中的文件名部分
---@return bee.fspath self
function path:remove_filename()
end

---替换路径中的文件名部分
---@param replacement bee.fspath|string 新的文件名
---@return bee.fspath self
function path:replace_filename(replacement)
end

---替换路径中的扩展名部分
---@param replacement bee.fspath|string 新的扩展名
---@return bee.fspath self
function path:replace_extension(replacement)
end

---返回词法上规范化的路径
---@return bee.fspath
function path:lexically_normal()
end

---文件状态对象
---@class bee.file_status
local file_status = {}

---获取文件类型
---@return "none"|"not_found"|"regular"|"directory"|"symlink"|"block"|"character"|"fifo"|"socket"|"junction"|"unknown"
function file_status:type()
end

---判断文件是否存在
---@return boolean
function file_status:exists()
end

---判断是否为目录
---@return boolean
function file_status:is_directory()
end

---判断是否为普通文件
---@return boolean
function file_status:is_regular_file()
end

---目录条目对象
---@class bee.directory_entry
local directory_entry = {}

---获取条目的路径
---@return bee.fspath
function directory_entry:path()
end

---刷新条目的缓存状态
function directory_entry:refresh()
end

---获取条目的文件状态
---@return bee.file_status
function directory_entry:status()
end

---获取条目的符号链接状态(不跟踪符号链接)
---@return bee.file_status
function directory_entry:symlink_status()
end

---获取条目的文件类型
---@return "none"|"not_found"|"regular"|"directory"|"symlink"|"block"|"character"|"fifo"|"socket"|"junction"|"unknown"
function directory_entry:type()
end

---判断条目是否存在
---@return boolean
function directory_entry:exists()
end

---判断条目是否为目录
---@return boolean
function directory_entry:is_directory()
end

---判断条目是否为普通文件
---@return boolean
function directory_entry:is_regular_file()
end

---获取条目的最后修改时间
---@return integer # 时间戳，单位为秒 (Unix时间戳)
function directory_entry:last_write_time()
end

---获取条目的文件大小
---@return integer # 文件大小，单位为字节
function directory_entry:file_size()
end

---创建路径对象
---@param p? string|bee.fspath 路径字符串或路径对象
---@return bee.fspath
function fs.path(p)
end

---获取文件状态
---@param p bee.fspath|string 路径
---@return bee.file_status
function fs.status(p)
end

---获取符号链接状态(不跟踪符号链接)
---@param p bee.fspath|string 路径
---@return bee.file_status
function fs.symlink_status(p)
end

---判断文件或目录是否存在
---@param p bee.fspath|string 路径
---@return boolean
function fs.exists(p)
end

---判断是否为目录
---@param p bee.fspath|string 路径
---@return boolean
function fs.is_directory(p)
end

---判断是否为普通文件
---@param p bee.fspath|string 路径
---@return boolean
function fs.is_regular_file(p)
end

---获取文件大小
---@param p bee.fspath|string 路径
---@return integer # 文件大小，单位为字节
function fs.file_size(p)
end

---创建单个目录
---@param p bee.fspath|string 路径
---@return boolean # 是否创建成功(目录已存在时返回false)
function fs.create_directory(p)
end

---递归创建目录
---@param p bee.fspath|string 路径
---@return boolean # 是否创建成功(目录已存在时返回false)
function fs.create_directories(p)
end

---重命名或移动文件/目录
---@param from bee.fspath|string 源路径
---@param to bee.fspath|string 目标路径
function fs.rename(from, to)
end

---删除文件或空目录
---@param p bee.fspath|string 路径
---@return boolean # 是否删除成功(不存在时返回false)
function fs.remove(p)
end

---递归删除文件或目录
---@param p bee.fspath|string 路径
---@return integer # 删除的文件/目录数量
function fs.remove_all(p)
end

---获取或设置当前工作目录
---@param p? bee.fspath|string 如果提供则设置为新的工作目录
---@return bee.fspath? # 不带参数时返回当前工作目录
function fs.current_path(p)
end

---复制文件或目录
---@param from bee.fspath|string 源路径
---@param to bee.fspath|string 目标路径
---@param options? integer 复制选项，参见 fs.copy_options
function fs.copy(from, to, options)
end

---复制单个文件
---@param from bee.fspath|string 源路径
---@param to bee.fspath|string 目标路径
---@param options? integer 复制选项，参见 fs.copy_options
---@return boolean # 是否复制成功
function fs.copy_file(from, to, options)
end

---获取绝对路径
---@param p bee.fspath|string 路径
---@return bee.fspath
function fs.absolute(p)
end

---获取规范路径(解析符号链接和..等)
---@param p bee.fspath|string 路径
---@return bee.fspath
function fs.canonical(p)
end

---获取相对路径
---@param p bee.fspath|string 路径
---@param base? bee.fspath|string 基准路径，默认为当前工作目录
---@return bee.fspath
function fs.relative(p, base)
end

---获取或设置文件的最后修改时间
---@param p bee.fspath|string 路径
---@param time? integer 时间戳，单位为秒 (Unix时间戳)
---@return integer? # 不带time参数时返回最后修改时间(秒)
function fs.last_write_time(p, time)
end

---获取或设置文件权限
---@param p bee.fspath|string 路径
---@param perms? integer 权限值
---@param options? integer 权限选项，参见 fs.perm_options
---@return integer? # 不带perms参数时返回当前权限
function fs.permissions(p, perms, options)
end

---创建符号链接(文件)
---@param target bee.fspath|string 目标路径
---@param link bee.fspath|string 链接路径
function fs.create_symlink(target, link)
end

---创建符号链接(目录)
---@param target bee.fspath|string 目标路径
---@param link bee.fspath|string 链接路径
function fs.create_directory_symlink(target, link)
end

---创建硬链接
---@param target bee.fspath|string 目标路径
---@param link bee.fspath|string 链接路径
function fs.create_hard_link(target, link)
end

---获取临时目录路径
---@return bee.fspath
function fs.temp_directory_path()
end

---获取文件系统空间信息
---@param p bee.fspath|string 路径
---@return bee.space_info
function fs.space(p)
end

---文件系统空间信息
---@class bee.space_info
---@field capacity integer 总容量，单位为字节
---@field free integer 空闲空间，单位为字节
---@field available integer 可用空间，单位为字节

---遍历目录(非递归)
---@param dir bee.fspath|string 目录路径
---@param options? integer 遍历选项，参见 fs.directory_options
---@return fun(): bee.fspath?, bee.directory_entry? iterator # 迭代器函数
---@return nil
---@return nil
---@return any to_be_closed # 可关闭对象
function fs.pairs(dir, options)
end

---递归遍历目录
---@param dir bee.fspath|string 目录路径
---@param options? integer 遍历选项，参见 fs.directory_options
---@return fun(): bee.fspath?, bee.directory_entry? iterator # 迭代器函数
---@return nil
---@return nil
---@return any to_be_closed # 可关闭对象
function fs.pairs_r(dir, options)
end

---复制选项
---@class bee.copy_options
---@field none integer 无特殊选项
---@field skip_existing integer 跳过已存在的文件
---@field overwrite_existing integer 覆盖已存在的文件
---@field update_existing integer 仅当源文件较新时覆盖
---@field recursive integer 递归复制目录
---@field copy_symlinks integer 复制符号链接而非其目标
---@field skip_symlinks integer 跳过符号链接
---@field directories_only integer 仅复制目录结构
---@field create_symlinks integer 创建符号链接而非复制
---@field create_hard_links integer 创建硬链接而非复制
fs.copy_options = {}

---权限选项
---@class bee.perm_options
---@field replace integer 替换权限
---@field add integer 添加权限
---@field remove integer 移除权限
---@field nofollow integer 不跟踪符号链接
fs.perm_options = {}

---目录遍历选项
---@class bee.directory_options
---@field none integer 无特殊选项
---@field follow_directory_symlink integer 跟踪目录符号链接
---@field skip_permission_denied integer 跳过权限拒绝的条目
fs.directory_options = {}

return fs
