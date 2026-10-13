-- sample_book_common.lua  秘籍物品公共逻辑
-- 示例共享模块：展示 require 的用法（数据驱动模式）
-- 两个秘籍（内功秘籍、剑法秘籍）共用此模块的 create 函数。
-- 用法：local book = require("item.sample_book_common")

local M = {}

-- 通用的秘籍创建函数：接收配置表，设置所有秘籍通用属性
-- data = { name, long, value, skill_name, exp_required, jing_cost, difficulty, max_skill }
function M.create(self, nFlag, data)
    self:set_name(data.name)
    self:set_weight(200)

    self:set("unit", "本")
    self:set("long", data.long)
    self:set("value", data.value)
    self:set("material", "paper")
    self:set("skill/name", data.skill_name)
    self:set("skill/exp_required", data.exp_required)
    self:set("skill/jing_cost", data.jing_cost)
    self:set("skill/difficulty", data.difficulty)
    self:set("skill/max_skill", data.max_skill)
    self:set("skill/min_skill", 0)
end

return M
