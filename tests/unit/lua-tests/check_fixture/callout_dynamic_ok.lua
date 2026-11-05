-- 防线5 盲区形态：目标名是变量（非字面量）→ 静态不可判，跳过不报
local npc = {}

function npc.go(self, target_name)
    self:call_out(target_name, 1)
end

return npc
