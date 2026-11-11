-- sample_blind_box.lua  盲盒
-- 示例物品：展示全局函数 xrandom 的用法——拆盒时从奖池里抽一件物品。
--
-- 关键点：
--   1. 奖池按 value 升序排（最值钱的排最后），xrandom 偏向小值：
--      落点在区间最低 20% 的概率约 40%，最高 20% 的只有约 5%——
--      数组位置即概率，价值越高越靠后、越难开出，脚本不再另算权重
--   2. 抽中后 load("item", ...) 取货、move 给使用者，随后销毁盲盒自身
--   3. 开盒即记档：谁/何时/开出什么写进独立表，回执私发本人
--      （记档链见 data/box_draw_log.lua）

-- 奖池：lua-sample 定义的全部物品，按 value 升序。
-- 不含 sample_book_common——那是 require 共享模块，不是可实例化的物品；
-- 也不含盲盒自己（盒不开盒）。
local prize_pool = {
    "sample_shedan", -- 0
    "sample_teapot", -- 50
    "sample_baozi", -- 200
    "sample_sword_basic", -- 200
    "key", -- 200
    "sample_heal", -- 300
    "sample_hp", -- 500
    "sample_xp", -- 500
    "sample_book_force", -- 500
    "sample_book_sword", -- 800
    "sample_teleport", -- 1000
    "sample_chess", -- 2000
    "sample_sword_adv", -- 2000
}

local draw_log = require("data.box_draw_log")

return {
    -- create(self, nFlag)
    create = function(self, nFlag)
        self:set_name("盲盒", "mang he")
        self:set("unit", "个")
        self:set("long", "一个看不出内里的木盒，凑近了能听见里面有东西轻轻作响。")
        self:set("value", 500)
    end,

    -- 声明可使用：客户端据此显示「使用」选项，use 命令也据此放行
    Can_Use = function(self)
        return 1
    end,

    -- do_use(self, me, target)：target 未用到——拆盒只取决于奖池
    do_use = function(self, me, target)
        local name = prize_pool[xrandom(#prize_pool) + 1]
        local prize = load("item", name)
        if not prize then
            return notify_fail("盲盒受了潮，死活打不开。")
        end

        prize:move(me)
        message("vision", "$N掀开盲盒的盖子，从里面拿出了$n。", me, prize)

        -- 记档：写独立表并读回累计次数，回执只发开盒人（失败也私发，见模块头注）
        local n, err = draw_log.record(me:name(), prize:name())
        if n then
            tell_object(me, string.format("此盒已记档：你第 %d 次开盲盒。", n))
        else
            tell_object(me, "此盒记档未成：" .. (err or "未知错误"))
        end

        destruct(self)
        return 1
    end,
}
