-- 夹具：带 on_moved / owner_is_killed 钩子的物品。
-- 两者都走「0 返回值 pcall」：成功路径必须只弹脚本表，多弹一个即栈下溢。
return {
    type = "item",
    create = function(self, flag)
        self:set_name("钩子物品", "li_hook")
        self:set("li_created", 1)
    end,

    on_moved = function(self, me)
        self:set("moved_count", 1)
    end,

    owner_is_killed = function(self, killer)
        self:set("killed", 1)
    end,
}
