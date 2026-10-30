-- sample_iron_dummy.lua  半山腰铁人桩
-- 示例NPC：展示「耐打的演武对手」——与 sample_wooden_dummy（对练用、血低
-- 易散）分工：铁人桩皮糙肉厚、不配技能（战力极低、还手不疼），专门喂招给
-- 需要多回合施放的绝招（如剑气三连）。被打散即自毁、房间 reset 重载，
-- 与木人同一生命周期（die 钩子自毁、不掉尸体不结 NK）。
--
-- 与木人的取舍写在这里免得后人混用：
--   1. 木人会被 /ask 对练抄成挑战者的镜像（mirror_base.make_mirror），血量
--      随挑战者变化、且一两回合就散——只适合短平快的连击演示；
--   2. 铁人桩不接对练（do_talk 未注册任何问话，走 base_do_talk 的
--      「对方并不愿意理你。」），属性固定，给长回合的绝招施放当沙包。

local npc = {}

function npc.create(self)
    self:set_name("铁人桩", "tie ren zhuang")
    self:set(
        "long",
        "一个铁铸的演武桩，通体黝黑、关节处缠着旧麻绳，胸口被打出一个个"
            .. "浅坑——看得出常有人拿它喂招。"
    )

    self:set("attitude", "peaceful")
    self:set("age", 1)
    self:set("level", 1)
    self:set("combat_exp", 100)

    -- 耐打：绝招三连加上战斗中的常规攻击要打上几回合才散，
    -- 血量给足才不会在施放者攒行动点的窗口里先被打散
    self:set("max_hp", 2000)
    self:set("hp", 2000)

    -- 不配技能（不 map 任何 skill）：战力走「无技能回退」的素人档，
    -- 还手伤害对练功弟子不构成威胁
    self:set("str", 10)
    self:set("con", 10)
    self:set("dex", 10)
    self:set("int", 10)
end

-- 打散自毁（与木人同款）：不走 die_base，不掉尸体、不结算 NK 经验
function npc.die(self)
    message_vision("$N哐当一声散成了一地铁片。", self)
    destruct(self)
end

return npc
