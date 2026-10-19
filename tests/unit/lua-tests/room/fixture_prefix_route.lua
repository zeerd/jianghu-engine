-- fixture_prefix_route.lua  测试夹具：动态实例房间的专属存盘表路由
--
-- 声明 prefix="fxroute"、can_save=true 与 save_table="t_fxroute"，验证工厂
-- 在 load() 之前把脚本 save_table 镜像进 dbase，使存/取/锁按声明表路由；
-- C++ 不认识 "t_fxroute" 这个数据集专用表名。
-- 本文件位于 lua-tests（测试根），不进入游戏数据集。
return {
    type = "room",
    name = "测试路由房间",
    prefix = "fxroute",
    can_save = true,
    save_table = "t_fxroute",

    create = function(self)
        self:set("fx_route_created", 1)
    end,
}
