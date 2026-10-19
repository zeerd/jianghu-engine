-- inherit_override_sample.lua  同名覆盖样本
--
-- 用途：验证「本技能自己注册的法术优先于继承来的同名法术」——
--       本技能声明可当投掷用，并自带一个「偷袭」，该实现必须压过
--       基本投掷继承来的「偷袭」。
return {
    valid_enable = function(self, usage)
        return (usage == "throwing") and 1 or 0
    end,

    create = function(self)
        self:set_name("覆盖投掷")
        self:add_magic("sneak", "自定偷袭", 10, 100, CAST_TO_OTHER, CAST_IN_BOTH, "do_sneak")
    end,

    do_sneak = function(self, me, target)
        return 2
    end,
}
