-- 防线5 反例：目标函数何处都未定义 → 注册必拒
local npc = {}

function npc.go(self)
    self:call_out("ghost_cb", 1)
end

return npc
