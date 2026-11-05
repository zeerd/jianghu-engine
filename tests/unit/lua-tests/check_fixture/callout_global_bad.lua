-- 防线5 反例：延迟函数写成顶层裸 function（全局），不在返回表里
-- → 注册期两处表都查不到，call_out 必拒（docs/guide/call_out.md 规则1）
local npc = {}

function legacy_cb(self, p1, p2) end  -- 全局定义，不是 npc 表字段

function npc.go(self)
    self:call_out("legacy_cb", 1)
end

return npc
