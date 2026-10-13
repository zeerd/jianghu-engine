-- t_error_item.lua  测试夹具：每个回调都抛错的物品
-- 供 tests/test_lua_error_paths.cpp 验证 CLuaItemT 的报错契约。
-- create 先置好名字再抛错，物件仍可被加载器产出，仅报错分支被触发。
local item = {}

function item.create(self, flag)
    self:set_name("报错包裹", "error item")
    self:set("long", "一件专门用来演示脚本报错的物品。")
    error("create boom")
end

function item.do_use(self, me, target)
    error("do_use boom")
end

function item.do_look(self, me)
    error("do_look boom")
end

function item.Can_Use(self)
    error("Can_Use boom")
end

function item.Can_Equip(self, me)
    error("Can_Equip boom")
end

function item.hit_ob(self, me, victim, damage_bonus)
    error("hit_ob boom")
end

function item.query_autoload(self)
    error("query_autoload boom")
end

function item.handle_action(self, me, comm, arg)
    error("handle_action boom")
end

function item.handle_action4(self, me, comm, arg, tar)
    error("handle_action4 boom")
end

function item.owner_is_killed(self, killer)
    error("owner_is_killed boom")
end

function item.do_action(self, act, me)
    error("do_action boom")
end

function item.on_moved(self, me)
    error("on_moved boom")
end

function item.init(self)
    error("init boom")
end

return item
