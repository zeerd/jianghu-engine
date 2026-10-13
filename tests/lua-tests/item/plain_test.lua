-- plain_test.lua  测试夹具：无 base 字段的普通脚本物品（type 用作基类选择回退）
return {
    type = "food",
    create = function(self, flag)
        self:set_name("测试干粮", "plain_test")
        self:set("value", 5)
    end,
}
