-- sample_book_sword.lua  剑法秘籍
-- 示例物品：展示秘籍类物品的简单实现
-- 使用 require 引入秘籍公共模块，复用 create 逻辑

local book = require("item.sample_book_common")

return {
    -- create(self, nFlag)
    create = function(self, nFlag)
        book.create(self, nFlag, {
            name = "剑法秘籍",
            long = "这是一本剑法入门秘籍，封面上画着一个持剑的小人。",
            value = 800,
            skill_name = "sword",
            exp_required = 2000,
            jing_cost = 25,
            difficulty = 15,
            max_skill = 50,
        })
    end,
}
