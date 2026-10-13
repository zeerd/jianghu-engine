-- treasure_test.lua  测试夹具：treasure 类型物品（套装骨架）
return {
    type = "treasure",
    base = "equip",
    save_table = "t_treasure",
    save_key = "unique_id",

    create = function(self, flag)
        self:set_name("测试宝刀", "treasure_test")
        self:set("index", 3)
        self:set("value", 100)
    end,
}
