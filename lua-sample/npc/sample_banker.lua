-- sample_banker.lua  山脚钱庄老板
-- 示例NPC：展示 type="bankowner" 钱庄老板（存款/取款/余额/贷款链路）

local npc = {}
npc.type = "bankowner"

function npc.create(self)
    self:set_name("钱庄老板", "lao ban")

    self:set("gender", "男性")
    self:set("age", 50)
    self:set("long", "一位须发花白的老掌柜，戴着老花镜，正慢条斯理地拨弄着算盘。")
    self:set("attitude", "friendly")
    self:set("no_kill", 1)

    self:set("combat_exp", 5000)
end

return npc
