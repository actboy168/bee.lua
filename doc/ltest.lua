---@meta ltest

local lt = {}

---@generic T
---@param value T
---@param errmsg? string
---@return T
function lt.assertIsUserdata(value, errmsg)
end

return lt
