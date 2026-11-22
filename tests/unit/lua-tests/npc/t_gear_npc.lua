-- t_gear_npc.lua  配装 NPC 夹具
--
-- create 每跑一次就给自己配一刀一衣。用于钉住脚本热重载的随身物契约：
-- reload_script 的默认分支是 purge() + create()，而 CContainer::purge()
-- 只删存档行、不清背包——所以 create() 必须幂等地看待已有随身物，
-- 重载后 NPC 身上仍只应有一刀一衣。若叠着配发，死后 make_corpse 会把
-- 多份一起倒出来（表现为"杀一次掉好几把"）。
local npc = {}

function npc.create(self)
    self:set_name("佩刀汉子", "t_gear_npc")
    self:set("long", "腰里别着刀、身上穿着衣的汉子。")
    self:set("hp", 100)
    self:set("max_hp", 100)

    self:carry_object("t_gear_sword")
    self:carry_object("t_gear_cloth")
end

return npc
