-- sample_arena_master.lua  山脚演武教头
-- 示例NPC：展示「脚本驱动引擎战斗房」的完整链路——普通战斗由 /kill、/fight
-- 的 C++ 路径开房，本教头则用 Lua 绑定自己开一场：
--   load_item("fight_room") 建房 → is_fight_room 验型 → Can_Join 验名额
--   → set_single 单人比试 → Join_Team 双方入队 → Begin_Fight 开打
--   → 战报（get_fight_team_num / get_fight_team_char / get_team_list /
--     Select_Member 点名）→ 数拍后 Leave_Team 退队 → End_Fight 收场。
-- 比试结束双方自动回到演武场（end_fight 把房内角色移回环境）。
--
-- 平衡设定：教头不配技能（战力走素人回退档）且历练为 0——几乎打不伤人，
-- 自己皮厚（2000 血）挨打不倒。比试输赢不重要，「点到为止」的收场时机
-- 才是脚本说了算的部分。

local npc = {}

function npc.create(self)
    self:set_name("演武教头", "yan wu")
    self:set(
        "long",
        "一位须发灰白的老教头，负手立在演武圈旁，眼神锐利——"
            .. "谁来挑战，他都奉陪。"
    )

    self:set("gender", "男性")
    self:set("age", 60)
    self:set("attitude", "peaceful")
    self:set("combat_exp", 0)
    self:set("max_hp", 2000)
    self:set("hp", 2000)

    self:set("str", 10)
    self:set("con", 10)
    self:set("dex", 10)
    self:set("int", 10)
end

-- 问答入口：命中「比武」开一场脚本驱动的比试，其余问话交回通用流程
function npc.do_talk(self, me, ask)
    if ask ~= "" and ask ~= "比武" and ask ~= "切磋" then
        return self:base_do_talk(me, ask)
    end

    local env = self:environment()
    local fr = load_item("fight_room")
    if not fr then
        return notify_fail("教头摆摆手：比武房一时开不出来，改日再来。")
    end
    if not fr:is_fight_room() then
        return notify_fail("教头皱眉：这不是比武房，开不得比试。")
    end
    if not fr:Can_Join(FIGHT_TEAM_A) or not fr:Can_Join(FIGHT_TEAM_B) then
        return notify_fail("教头道：比武房满了，稍后再来。")
    end

    fr:set_name("演武比试")
    fr:set_single(true)
    fr:move(env)

    me:move(fr)
    self:move(fr)
    fr:Join_Team(me, FIGHT_TEAM_A)
    fr:Join_Team(self, FIGHT_TEAM_B)

    -- 战况播报：私发给挑战者。A/B 两队名册与点名都走引擎战斗房查询绑定，
    -- 战报里带双方名字（say 第二参收件人，仅挑战者可见）
    local foe = fr:Select_Member(FIGHT_TEAM_A)
    local mates = get_team_list(self, FIGHT_TEAM_B, nil, 4) or {}
    local mate_names = {}
    for i = 1, #mates do
        mate_names[#mate_names + 1] = mates[i]:name()
    end
    self:say(
        string.format(
            "演武比试开打！A队%d人（%s），B队%d人（%s）；%s下场开打。",
            get_fight_team_num(me),
            foe and foe:name() or "无",
            get_fight_team_num(self),
            table.concat(mate_names, "、"),
            foe and foe:name() or "对手"
        ),
        me
    )

    fr:Begin_Fight(0)
    -- 数拍后点到为止：退己方 + 收比武房，全由脚本收场（引擎心跳不会抢先
    -- ——双方都活着，空队/血量结束条件都不满足）
    self:call_out("yield", 6)
    return 1
end

-- 点到为止：战报收尾（get_fight_team_char 取本队头名）→ 退队 → 收场
function npc.yield(self, param1, param2)
    local fr = self:environment()
    if not fr or not fr:is_fight_room() then
        return
    end

    local head = get_fight_team_char(self, 0)
    self:say(
        string.format(
            "教头跳出战圈，拱手道：点到为止！本队头名是%s。",
            head and head:name() or "无"
        ),
        nil
    )
    fr:Leave_Team(self)
    fr:End_Fight()
end

-- 不做 die 钩子：教头 2000 血不会在比试窗口内被打散，走标准死亡非本段语义
return npc
