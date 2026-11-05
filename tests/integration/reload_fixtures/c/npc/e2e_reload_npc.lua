-- e2e_reload_npc.lua  热重载 E2E 专用 NPC（fixture，变体 c）
local npc = {}

function npc.create(self)
    self:set_name("重载测试员", "e2e_reload_npc")
    self:set("gender", "男性")
    self:set("long", "热重载测试专用的接待员。")
    self:set_inquiry("重载", "【NPC丙】重载测试应答。")
end

return npc
