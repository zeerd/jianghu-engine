-- qingyun_xinfa.lua  青云心法
-- 示例技能：展示「知识类武学」（type="knowledge"）——不入战斗链、不占
-- enable/prepare 槽位，valid_learn 不设武器与基础门槛，入门只看师承。
--   本门弟子向青云子请教入门（sample_master）；判师改投时被散去
--   （sample_heifeng_master 的「判师」问话：全清本门所学）。

return {
    create = function(self)
        self:set_name("青云心法")
    end,

    valid_learn = function(self, me)
        return 1
    end,

    type = function(self)
        return "knowledge"
    end,
}
