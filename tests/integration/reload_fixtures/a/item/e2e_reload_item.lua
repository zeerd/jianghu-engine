-- e2e_reload_item.lua  热重载 E2E 专用物品（fixture，变体 a）
-- do_use 不 destruct、不置 busy：同一实例可跨片段反复使用（变体切换靠
-- 重载通知刷新实例，不靠重新获取）。
return {
    type = "food",

    create = function(self, nFlag)
        self:set_name("重载药", "e2e_reload_item")
        self:set("unit", "剂")
        self:set("long", "【物品甲】一剂测试用药。")
        self:set("value", 100)
    end,

    -- 声明可使用：客户端据此显示「使用」选项，use 命令也据此放行
    Can_Use = function(self)
        return 1
    end,

    do_use = function(self, me, target)
        message_vision("$N服下一剂重载药，【物品甲】药力发作。", me)
        return 1
    end,
}
