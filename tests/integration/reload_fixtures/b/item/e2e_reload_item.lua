-- e2e_reload_item.lua  热重载 E2E 专用物品（fixture，变体 b）
return {
    type = "food",

    create = function(self, nFlag)
        self:set_name("重载药", "e2e_reload_item")
        self:set("unit", "剂")
        self:set("long", "【物品乙】一剂测试用药。")
        self:set("value", 100)
    end,

    Can_Use = function(self)
        return 1
    end,

    do_use = function(self, me, target)
        message_vision("$N服下一剂重载药，【物品乙】药力发作。", me)
        return 1
    end,
}
