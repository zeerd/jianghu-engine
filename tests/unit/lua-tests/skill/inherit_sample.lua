-- inherit_sample.lua  法术继承样本
--
-- 用途：验证「高级技能 enable 到某技能种类后，基础技能自带的法术也注册
--       到高级技能上」这条引擎规则。
--
-- 本技能声明可当投掷用（valid_enable），自己不注册任何法术：
-- create 之后应当继承到基本投掷的「偷袭」，且偷袭由基本投掷的脚本执行。
return {
    valid_enable = function(self, usage)
        return (usage == "throwing") and 1 or 0
    end,
}
