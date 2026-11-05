-- e2e_reload_npc.lua  热重载 E2E 专用 NPC（fixture，变体 d）
-- 与 c 变体行为完全一致（应答同为丙），但字节不同：链内 heal/恢复片段必须
-- 交替使用 c/d——自动重载先过 mtime 快筛、再比内容哈希，重复写入同一内容
-- 会被判成「mtime 抖动、内容未变」的 noop，等不到成功广播。
local npc = {}

function npc.create(self)
    self:set_name("重载测试员", "e2e_reload_npc")
    self:set("gender", "男性")
    self:set("long", "热重载测试专用的接待员。")
    self:set_inquiry("重载", "【NPC丙】重载测试应答。")
end

return npc
