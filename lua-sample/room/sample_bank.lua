-- sample_bank.lua  山脚钱庄
-- 示例房间：展示 type="bankowner" 钱庄老板 NPC 的用法
-- 玩家可对钱庄老板执行 deposit/withdraw/balance/loan 命令。
--
-- 本样本另演示「房间的两个名字」：顶层 name 与 set_name 刻意取不同值
-- （山脚钱庄 / 钱庄）——寻址走长名，抬头看短名。
--
-- 【注册名】顶层 name = "山脚钱庄"（全服唯一）
--   · 房间名索引按它建立：别的房间把目标房写成 add_object("door",
--     "山脚钱庄", "山脚钱庄", …)，第二参 next_room 由 load_room 解析回本脚本；
--     load("room", …) 与全局房间表 global_room_name(i) 同样按它查；
--   · 重名会撞索引：两间房共用一个注册名时索引只留其中一个，
--     另一个的中文名会解析到错误的脚本（数据集侧由房间引用校验拦重名）。
--
-- 【显示名】create 里的 self:set_name("钱庄")
--   · 只写进房间对象自己的属性表：客户端的房间标题、房间无主发言 say、
--     「XX离开了XX」这类房间播报、组队经验共享的「同房间」判定都读它；
--   · 不参与寻址：load("room", "钱庄") 解析不到，也没有任何一扇门认它。
--
-- 两个名字互不回填：顶层 name 不进房间对象，set_name 不进房间名索引。
-- set_name 的第二参是英文 id（/t 定向用），房间不传就回落 base_name。
--
-- 门的四个名字见 sample_mountain_foot.lua 的出口列表注释。
-- 本房回到山脚的门（第一参门id 即导航键）：/go 青云山山脚。

local common = require("room.sample_room_common")

return {
    type = "room",
    name = "山脚钱庄",

    create = function(self)
        -- 显示名（短名）：与顶层注册名不同值，进房抬头看到的是它
        self:set_name("钱庄")
        self:add_object("npc", "sample_banker")
        self:add_object("door", "青云山山脚", "青云山山脚", "山脚钱庄")
        self:set("no_fight", 1)

        self:set(
            "long",
            "一间不大的钱庄，门口挂着「童叟无欺」的匾额。"
                .. "柜台后坐着一位老掌柜，正拨弄着算盘。"
        )
    end,

    do_look = function(self, me)
        return common.do_look(self, me)
    end,
}
