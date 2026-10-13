-- sample_book_force.lua  内功秘籍
-- 示例物品：展示秘籍类物品的简单实现
-- 使用 require 引入秘籍公共模块，复用 create 逻辑

local book = require("item.sample_book_common")

return {
    -- create(self, nFlag)
    create = function(self, nFlag)
        book.create(self, nFlag, {
            name = "内功秘籍",
            long = "这是一本讲述内功修炼基础的秘籍，封面写着「内功心法入门」。",
            value = 500,
            skill_name = "force",
            exp_required = 1000,
            jing_cost = 20,
            difficulty = 10,
            max_skill = 30,
        })
    end,
}
