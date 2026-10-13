-- sample_pheasant.lua  山鸡
-- 示例NPC：展示低级练级怪的简单实现。
-- 击杀奖励走 C++ 挂点 hooks.kill_bonus（见 data/hooks.lua），
-- 挂点对每次玩家击杀都会被调用（含 nk_gain 算出 0 的情况），
-- 击杀者固定获得 10000 经验，供新玩家练级。

local npc = {}

function npc.create(self)
    self:set_name("山鸡", "shan ji")
    self:set("race", "野兽")
    self:set("age", 1)
    self:set("long", "一只羽毛斑斓的山鸡，正在草丛间刨土觅食。")

    self:set("level", 1)
    self:set("combat_exp", 100)

    self:set("max_hp", 300)
    self:set("hp", 300)

    self:set("xp_bonus", 1)

    -- 野兽的攻击动作白名单（战斗时随机选用其一）；动物没有技能体系，
    -- 不能用 set_skill 赋予，动作必须取自内置动作表 {hoof,bite,claw,poke,knock}
    self:set_beast_verbs({ "poke" })
end

return npc
