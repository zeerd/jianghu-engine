-- e2e_reload_npc.lua  热重载 E2E 专用 NPC（fixture，变体 a）
-- 由 tests/integration/reload_fixtures/<变体>/ 提供，剧本经 filecopy 切换到
-- 私有脚本副本。断言只认自己的标记文本（NPC甲/乙/丙），不依赖教材文案，
-- 教材演进不牵动本夹具。不定义 reload 钩子 → 走默认重载路径（purge+create）。
local npc = {}

function npc.create(self)
    self:set_name("重载测试员", "e2e_reload_npc")
    self:set("gender", "男性")
    self:set("long", "热重载测试专用的接待员。")
    self:set_inquiry("重载", "【NPC甲】重载测试应答。")
end

return npc
