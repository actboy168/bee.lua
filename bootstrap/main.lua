local expr = {}

local i = 1
while true do
    if arg[i] and arg[i]:sub(1,1) == '-' then
        if arg[i] == '-e' then
            i = i + 1
            expr[#expr+1] = assert(arg[i], "'-e' needs argument")
        end
    else
        break
    end
    i = i + 1
end

arg[-i] = arg[-1]
for j = 1, #arg do
    arg[j - i] = arg[j]
end
for j = #arg - i + 1, #arg do
    arg[j] = nil
end

for _, e in ipairs(expr) do
    assert(load(e))()
end
if arg[0] == nil then
    return
end
assert(loadfile(arg[0]))(table.unpack(arg))
