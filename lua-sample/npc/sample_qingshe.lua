-- sample_qingshe.lua  青蛇
-- 示例NPC：展示低级练级怪的简单实现。
-- 击杀奖励走 C++ 挂点 hooks.kill_bonus（见 data/hooks.lua），挂点对每次玩家
-- 击杀都会被调用（含 nk_gain 算出 0 的情况），击杀者固定获得 10000 经验。
-- 掉落在死亡本身：脚本接管 die，把蛇胆丢在脚下后回链标准死亡流程。

local npc = {}

function npc.create(self)
    self:set_name("青蛇", "qing she")
    self:set("race", "野兽")
    self:set("age", 1)
    self:set("long", "一条青色的蛇，眼神锐利，头部微微抬起。")

    self:set("level", 1)
    self:set("combat_exp", 100)

    self:set("max_hp", 300)
    self:set("hp", 300)

    self:set("xp_bonus", 1)

    -- 野兽的攻击动作白名单（战斗时随机选用其一）；动物没有技能体系，
    -- 不能用 set_skill 赋予，动作必须取自内置动作表 {hoof,bite,claw,poke,knock}
    self:set_beast_verbs({ "poke" })
end

-- 死亡：蛇胆丢在脚下，谁杀的谁自己捡。
-- 脚本定义 die 即整个接管死亡——不回链 die_base 就既不掉尸体、也不结算
-- 击杀经验（见 lua-default/npc/mirror_base.lua 的 die 装配注释），
-- 故顺序固定：先掉落，后 die_base 走标准死亡流程。
function npc.die(self)
    local env = self:environment()
    if env then
        local shedan = load_item("sample_shedan")
        if shedan then
            shedan:move(env)
            message_vision("$N身上掉出了一颗蛇胆。", self)
        end
    end

    self:die_base()
end

return npc
