-- sample_resident.lua  青云城住宅区
-- 示例房间：展示住宅区（申请居民 → 置产 → 搬家 → 卖房迁出）
--
-- 玩家在此可以：
--   1. 申请成为本城居民（地价走 resident_base 缺省阶梯价，年龄门槛同缺省）；
--   2. 选房型（单人间 / 双人套房），给房子起名、写描述；
--   3. 缴银登记房屋，登记后本区菜单里出现「回家」，可进入自己的宅院；
--   4. 查看本区住户名单、卖掉名下房产迁出本城。
--
-- 本脚本声明场景与政策：type = "resident" 即住宅区；购房登记、管理费
-- 结算、住户名录与卖房迁出由 require 进来的 room/resident_base 装配
-- （return 前调 install，见下）提供。
-- 地价不在此声明：走 resident_base 的缺省阶梯价（住户越多越贵）。

local resident_base = require("room.resident_base")

return resident_base.install({
    type = "resident",
    name = "青云城住宅区",

    create = function(self)
        self:set_name("青云城住宅区")
        self:add_door("青云山山脚", "青云山山脚", "青云城住宅区")
        self:set("resident", "青云")
        self:set("no_fight", 1)

        self:add_npc("sample_marriage", 1)

        self:set(
            "long",
            "沿街是一排青砖小院，门楣上贴着各家的名帖。院里传来扫叶的沙沙声，"
                .. "偶尔有仆役进出，向过往的行人颔首致意。"
        )
    end,
})
