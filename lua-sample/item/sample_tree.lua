-- sample_tree.lua  一个奇怪的树
-- 示例物品：演示「房间陈设物」这一类物件的边界与入口——
--   1. 陈设物不可被捡走（no_get）、也不随房间 reset 清扫（no_clean_up）：
--      它是场景的一部分，被搬走或几轮 reset 后消失，随机刷出的事件立不住；
--   2. 命令分发中「物件截获玩家命令」先于内建命令（见 CCmdHandle::command），
--      所以对它 talk 不走 CCmdUser::talk，而是进本脚本的 handle_action，
--      由脚本决定「和怪树聊天即摘果」；
--   3. 摘下的果子用 load("item", ...) 取货、move 交给玩家（与盲盒开盒同路）。

return {
    create = function(self, nFlag)
        self:set_name("一个奇怪的树", "tree")
        self:set("unit", "棵")
        self:set(
            "long",
            "一棵歪脖子怪树，枝头挂着一枚晶莹的果子。"
                .. "它似乎听得懂人话，凑近和它聊上几句，也许能把果子摘下来。"
        )
        self:set("no_get", 1) -- 陈设物：搬不走
        self:set("no_clean_up", 1) -- 不随房间 reset 清扫
    end,

    -- 对怪树的定向命令：只认 talk（摘果），其余交回常规命令分发
    handle_action = function(self, comm, me, arg)
        if comm ~= "talk" then
            return 0
        end

        if self:query("picked") ~= 0 then
            tell_object(me, "怪树抖了抖叶子，枝头已经没有果子了。")
            return 1
        end

        local fruit = load("item", "sample_xp")
        if not fruit then
            tell_object(me, "怪树抖了抖叶子，果子不知去了哪里。")
            return 1
        end

        fruit:move(me)
        self:set("picked", 1)
        message("vision", "$N伸手从怪树上摘下了$n。", me, fruit)
        return 1
    end,
}
