-- sample_mountain_foot.lua  青云山山脚
-- 示例房间：展示普通房间的简单实现
-- 使用 require 引入房间公共模块，复用 do_look 逻辑

local common = require("room.sample_room_common")

return {
    type = "room",
    name = "青云山山脚",

    create = function(self)
        self:set_name("青云山山脚")
        self:add_object("npc", "sample_guide")
        self:add_object("npc", "sample_vendor")
        -- 巡游NPC：起点放在路口（出口最多），random_move 由此走遍全图
        self:add_object("npc", "sample_wanderer")
        self:add_object("door", "青云山半山腰", "青云山半山腰", "青云山山脚")
        self:add_object("door", "山脚茶棚", "山脚茶棚", "青云山山脚")
        self:add_object("door", "山脚钱庄", "山脚钱庄", "青云山山脚")
        self:add_object("door", "青云城住宅区", "青云城住宅区", "青云山山脚")
        self:add_object("door", "山脚演武场", "山脚演武场", "青云山山脚")
        self:add_object("door", "山脚林间空地", "山脚林间空地", "青云山山脚")
        self:set("no_fight", 1)

        -- 放置箱子（set_name 第二参是 /t 用的英文 id：单参会被构造期的
        -- id 回落固化成空串，故两箱各给独立 id，当前房间内可精确定位）
        local box = self:add_object("box")
        box:add_object("object", "sample_sword_basic", 1)
        box:add_object("object", "coin", 60000)
        -- 棋子声明了 can_combine_in_box=false：暗器进箱不并堆，按盒取用
        box:add_object("object", "sample_chess", 1)
        box:set_name("木箱", "mu xiang")

        local box2 = self:add_object("box")
        box2:add_object("object", "sample_heal", 3)
        box2:set_name("药箱", "yao xiang")

        -- 锁着的宝箱：演示箱子上锁（locked 绑定设 status/reason/duration）。
        -- 铜锁无主（不传 owner，人人可持钥匙试开）；密码与商人卖的钥匙配套
        -- （key.lua 的 password 同值），开锁走 CBox::handle_action 的钥匙支。
        -- 箱中一册剑法秘籍，配得上一把锁。
        local box3 = self:add_object("box")
        box3:add_object("object", "sample_book_sword", 1)
        box3:set_name("锁着的箱子", "suo xiang")
        box3:set("password", 123456)
        box3:locked("一把铜锁锈得发红", 0)

        self:set(
            "long",
            "这里是青云山的山脚，一条蜿蜒的山路通向山顶。路边有一个杂货铺，还有一位老者在为路人指路。"
                .. "山脚东侧是一片青砖小院，那是青云城的住宅区。"
        )
    end,

    do_look = function(self, me)
        return common.do_look(self, me)
    end,
}
