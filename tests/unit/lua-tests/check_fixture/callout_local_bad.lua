-- 防线5 反例：local function 未导出到返回表 → 表里查不到，注册必拒
-- （locals 不是表字段，按名 call_out 找不到）
local npc = {}

local function hidden_cb(self, p1, p2) end

function npc.go(self)
    self:call_out("hidden_cb", 1)
end

return npc
