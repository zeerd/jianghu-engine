-- 故意的语法错误变体：filecopy 到 $scripts/npc/e2e_reload_npc.lua 后，
-- 心跳自动重载必须失败并保留旧版本（reload_script 的 loadfile 失败分支）。
local npc = {}
function npc.create(self)
    self:set_name("重载测试员", "e2e_reload_npc")
