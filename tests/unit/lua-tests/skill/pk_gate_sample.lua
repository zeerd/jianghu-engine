-- pk_gate_sample.lua  施法 PK 敌意判定测试技能
-- special 技能：不经技能映射/武器校验，直接以 `cast <本技能名> <法术名>` 施放。
-- 四个法术覆盖 hostile 标志 × target 形态的组合，用于锁定
-- CCmdStandard::cast 的 PK 敌意判定矩阵。
return {
    create = function(self)
        self:set_name("敌意判定示例")
        self:set("special", 1)

        -- 敌意（缺省）+ 指定他人：命中玩家受害者应挂 PK 标记
        self:add_magic("strike", "扑击", 30, 100, CAST_TO_OTHER, CAST_IN_BOTH, "do_ok")
        -- 非敌意 + 指定他人：对玩家施放也不挂 PK 标记
        self:add_magic("mend", "扶持", 30, 100, CAST_TO_OTHER, CAST_IN_BOTH, "do_ok", 0)
        -- 非敌意 + 无目标（自身姿态）：不挂 PK 标记
        self:add_magic("focus", "凝神", 30, 100, CAST_TO_NONE, CAST_IN_BOTH, "do_ok", 0)
        -- 敌意 + 无目标（波及全场，狮子吼一类）：无玩家受害者也挂 PK 标记
        self:add_magic("shout", "怒吼", 30, 100, CAST_TO_NONE, CAST_IN_BOTH, "do_ok")
    end,

    do_ok = function(self, me, target)
        return 1
    end,
}
