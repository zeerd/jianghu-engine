-- throw_adv_sample.lua  高级投掷样本
--
-- 用途：验证「enable 高级技能后，基础技能的法术也注册到高级技能上」——
--       本技能声明可当投掷用，自带一个「飞星连珠」，同时继承基本投掷的
--       「偷袭」。
return {
    valid_enable = function(self, usage)
        return (usage == "throwing") and 1 or 0
    end,

    create = function(self)
        self:set_name("样本高级投掷")
        self:add_magic("feixing", "飞星连珠", 80, 100, CAST_TO_OTHER, CAST_IN_COMBAT, "do_feixing")
    end,

    do_feixing = function(self, me, target)
        return 1
    end,
}
