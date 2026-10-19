-- sample_resident.lua  青云城住宅区
-- 示例房间：展示住宅区（申请居民 → 置产 → 搬家 → 卖房迁出）
--
-- 玩家在此可以：
--   1. 申请成为本城居民（年龄与地价门槛由本区自行声明，见下 policy）；
--   2. 选房型（单人间 / 双人套房），给房子起名、写描述；
--   3. 缴银登记房屋，登记后本区菜单里出现「回家」，可进入自己的宅院；
--   4. 查看本区住户名单、卖掉名下房产迁出本城。
--
-- 本脚本只声明场景与政策：type = "resident" 即住宅区，购房登记、管理费
-- 结算、住户名录与卖房迁出由引擎在脚本加载期自动装配
-- room/resident_base 提供（无需 require + install）。
--
-- 置产政策写在 policy 子映射里（缺席时用 resident_base 的缺省值）：
--   policy/min_age      准予置产的最低年龄（缺省 16）
--   policy/house_price  本区地价（金；缺省走阶梯地价，住户越多越贵）

local resident_base = require("room.resident_base")

-- 声明本区置产政策：示例世界的青云城是市井小城，地价低廉且不问年齿
local function set_policy(self)
    self:set("policy/min_age", 14)
    self:set("policy/house_price", 5000)
end

return {
    type = "resident",
    name = "青云城住宅区",

    create = function(self)
        self:set_name("青云城住宅区")
        self:add_door("青云山山脚", "青云山山脚", "青云城住宅区")
        self:set("resident", "青云")
        self:set("no_fight", 1)
        set_policy(self)

        self:set(
            "long",
            "沿街是一排青砖小院，门楣上贴着各家的名帖。院里传来扫叶的沙沙声，"
                .. "偶尔有仆役进出，向过往的行人颔首致意。"
        )
    end,

    -- 引擎载入存档时整体覆盖 dbase（旧存档可能没有 policy 这两项），
    -- 故在居民区基类 setup 之后重申一次本区政策。
    setup = function(self)
        resident_base.setup(self)
        set_policy(self)
    end,
}
