-- hitmsg_sample.lua  示例技能
-- hit_ob 返回 (damage, 命中消息) 双返回值：消息由战斗结算追加到战报
return {
    base = "force",
    create = function(self)
        self:base_create()
        self:set_name("高级内功")
        -- add_magic(name, displayName, mp, ap, target, validCast, luaFuncName)
        -- 自疗术：对自身施放，战斗中可用（非敌意，不挂 PK 标记）
        self:add_magic("heal", "自疗术", 50, 100, CAST_TO_ME, CAST_IN_BOTH, "do_heal", 0)
        -- 狮子吼：波及全场的攻击性震慑，属敌意行为
        self:add_magic("roar", "狮子吼", 200, 100, CAST_TO_NONE, CAST_IN_COMBAT, "do_roar")
    end,
    hit_ob = function(self, me, victim, damage)
        return damage, "示例技能的命中消息。"
    end,
}
