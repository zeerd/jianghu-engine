-- routed_nokey_test.lua  测试夹具：只声明 save_table、不声明 save_key 的物品
-- （路由到专属表，但 obj_id 仍取 base_name）
return {
    type = "equip",
    base = "equip",
    save_table = "t_routed",

    create = function(self, flag)
        self:set_name("无键路由件", "routed_nokey_test")
        self:set("value", 10)
    end,
}
