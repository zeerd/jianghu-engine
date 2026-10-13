-- enable_sample.lua  可被 enable 的示例技能
-- 供 CCmdStandard::enable 的成功路径使用：
-- base 技能为 force / unarmed 时放行，其余用法拒绝。
return {
    create = function(self)
        self:set_name("启用示例")
        self:set("type", "martial")
    end,

    valid_enable = function(self, usage)
        return (usage == "force" or usage == "unarmed") and 1 or 0
    end,
}
