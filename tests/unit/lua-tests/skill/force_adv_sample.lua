-- force_adv_sample.lua  高级内功样本
--
-- 用途：验证「enable 高级技能后，基础技能的法术也注册到高级技能上」——
--       本技能声明可当内功用，自带一个「自疗术」，同时继承基本内功的
--       「复元术」。
return {
    valid_enable = function(self, usage)
        return (usage == "force") and 1 or 0
    end,

    create = function(self)
        self:set_name("样本高级内功")
        self:add_magic("heal", "自疗术", 50, 100, CAST_TO_ME, CAST_IN_BOTH, "do_heal", 0)
    end,

    do_heal = function(self, me, target)
        return 1
    end,
}
