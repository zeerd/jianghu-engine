-- inherit_empty_sample.lua  法术继承的反向样本
--
-- 用途：验证「基础技能没有法术的技能种类，不产生幽灵法术」——
--       本技能声明可当拳脚用（valid_enable），基本拳脚没有任何法术，
--       create 之后本技能也不应有法术。
return {
    valid_enable = function(self, usage)
        return (usage == "unarmed") and 1 or 0
    end,
}
