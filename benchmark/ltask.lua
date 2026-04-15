local task = {}

local MESSAGE_OK <const> = 0
local MESSAGE_ERROR <const> = 1

local running_thread
local wakeup_queue = {}
local message_queue = {}
local thread_address = {}
local waiting = {}
local handler = {}

local function wakeup_thread(...)
    wakeup_queue[#wakeup_queue + 1] = { ... }
end

local function handle_message(from, command, ...)
    local s = handler[command]
    if not s then
        error("Unknown message : " .. command)
        return
    end
    wakeup_thread(from, MESSAGE_OK, s(...))
    thread_address[running_thread] = nil
end

local function do_wakeup(co, ...)
    running_thread = co
    local ok, errobj = coroutine.resume(co, ...)
    running_thread = nil
    if ok then
        return errobj
    else
        local from = thread_address[co]
        thread_address[co] = nil
        errobj = debug.traceback(co, errobj)
        if from == nil then
            print("Error:", tostring(errobj))
        else
            wakeup_thread(from, MESSAGE_ERROR, errobj)
        end
        coroutine.close(co)
    end
end

local function do_message(from, ...)
    local co = coroutine.create(handle_message)
    thread_address[co] = from
    do_wakeup(co, from, ...)
end

local function response(type, ...)
    if type == MESSAGE_OK then
        return ...
    else -- type == MESSAGE_ERROR
        error(...)
    end
end

local function send_message(from, ...)
    message_queue[#message_queue + 1] = { from, ... }
end

local function no_response_()
    while true do
        local type, errobj = coroutine.yield()
        if type == MESSAGE_ERROR then
            print("Error:", tostring(errobj))
        end
    end
end
local no_response_handler = coroutine.create(no_response_)
coroutine.resume(no_response_handler)

function task.wait(token)
    token = token
    waiting[token] = running_thread
    return response(coroutine.yield())
end

function task.wakeup(token, ...)
    local co = waiting[token]
    if co then
        wakeup_thread(co, MESSAGE_OK, ...)
        waiting[token] = nil
        return true
    end
end

function task.interrupt(token, errobj)
    local co = waiting[token]
    if co then
        errobj = debug.traceback(errobj)
        wakeup_thread(co, MESSAGE_ERROR, errobj)
        waiting[token] = nil
        return true
    end
end

function task.fork(func, ...)
    local co = coroutine.create(func)
    wakeup_thread(co, ...)
end

function task.yield()
    wakeup_thread(running_thread)
    coroutine.yield()
end

function task.call(command, ...)
    send_message(running_thread, command, ...)
    return response(coroutine.yield())
end

function task.send(command, ...)
    send_message(no_response_handler, command, ...)
end

function task.dispatch(h)
    for k, v in pairs(h) do
        handler[k] = v
    end
end

function task.schedule()
    local did_work = false
    -- 耗尽所有可运行任务，避免每条消息触发一次 I/O poll
    while #message_queue ~= 0 do
        local s = table.remove(message_queue, 1)
        do_message(table.unpack(s))
        did_work = true
    end
    while #wakeup_queue ~= 0 do
        local s = table.remove(wakeup_queue, 1)
        do_wakeup(table.unpack(s))
        did_work = true
        -- wakeup 可能产生新 message，立即处理
        while #message_queue ~= 0 do
            local s2 = table.remove(message_queue, 1)
            do_message(table.unpack(s2))
        end
    end
    return did_work or nil
end

return task
