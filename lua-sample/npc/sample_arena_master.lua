-- sample_arena_master.lua  山脚演武教头
-- 示例NPC：展示「脚本驱动引擎战斗房」的完整链路——普通战斗由 /kill、/fight
-- 的 C++ 路径开房，本教头则用 Lua 绑定自己开一场：
--   load("item", "fight_room") 建房 → query(IS_FIGHTROOM) 验型 → can_join 验名额
--   → set_single 单人比试 → join_team 双方入队 → begin_fight 开打
--   → 战报（select_member 点名本队对手）→ 数拍后 leave_team 退队 →
--     end_fight 收场（get_fight_team_char 报本队头名）。
-- 比试结束双方自动回到演武场（end_fight 把房内角色移回环境）。
--
-- 同一场比试还有第二条开法：「指教」走引擎的 do_bishi——建房、入队、开打
-- 三步由绑定一把梭（见 do_bishi），阈值 check 定「打到几成血算输」。两条路
-- 殊途同归，差别只在谁来装配战斗房：上面那条逐绑定演示装配过程，指教那条
-- 演示引擎替你装配。
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

-- 问答入口：命中「比武」开一场脚本驱动的比试、命中「指教」走引擎绑定一把梭；
-- 空问话（裸 /ask）列菜单告诉对方能约什么；其余问话交回通用流程
function npc.do_talk(self, me, ask)
    if ask == "指教" then
        return npc.do_bishi(self, me)
    end

    if ask ~= "比武" and ask ~= "切磋" then
        if ask == "" then
            self:add_menu_item("比武切磋", "$0比武 $1", me)
            self:add_menu_item("请教指教", "$0指教 $1", me)
            self:send_menu(me)
            return 1
        end
        return self:base_do_talk(me, ask)
    end

    local env = self:environment()
    local fr = load("item", "fight_room")
    if not fr then
        return notify_fail("教头摆摆手：比武房一时开不出来，改日再来。")
    end
    if fr:query(IS_FIGHTROOM) == 0 then
        return notify_fail("教头皱眉：这不是比武房，开不得比试。")
    end
    if not fr:can_join(FIGHT_TEAM_A) or not fr:can_join(FIGHT_TEAM_B) then
        return notify_fail("教头道：比武房满了，稍后再来。")
    end

    fr:set_name("演武比试")
    fr:set_single(true)
    fr:move(env)

    me:move(fr)
    self:move(fr)
    fr:join_team(me, FIGHT_TEAM_A)
    fr:join_team(self, FIGHT_TEAM_B)

    -- 战况播报：私发给挑战者（say 第二参收件人，仅挑战者可见）。
    -- 对手点名走引擎战斗房查询绑定 select_member（get_fight_team_char
    -- 留给收场时的本队头名点名，见下方 yield）
    local foe = fr:select_member(FIGHT_TEAM_A)
    self:say(
        string.format(
            "演武比试开打！%s 已下场，与老夫各率一队过招。",
            foe and foe:name() or "来者"
        ),
        me
    )

    fr:begin_fight(0)
    -- 数拍后点到为止：退己方 + 收比武房，全由脚本收场（引擎心跳不会抢先
    -- ——双方都活着，空队/血量结束条件都不满足）
    self:call_out("yield", 6)
    return 1
end

-- 「指教」：建房、入队、开打三步交给引擎的比试绑定 do_bishi，脚本不再逐个
-- 装配。check 是阈值收场的血线——「打到九成血算输」，谁先掉到九成以下，
-- 比试立刻判负收场（阈值判定在行动方回合开打之前，故几回合内必然见分晓）。
function npc.do_bishi(self, me)
    local room = do_bishi(self, me, "演武指教", 90)
    if not room then
        return notify_fail("教头摆摆手：今日比不得。")
    end

    -- say 第二参给收件人 = 私发；双方此刻已在比试房内，私发不看房间
    self:say("接招吧——打到九成血，就算你赢。", me)
    return 1
end

-- 点到为止：战报收尾（get_fight_team_char 取本队头名）→ 退队 → 收场
function npc.yield(self, param1, param2)
    local fr = self:environment()
    if not fr or fr:query(IS_FIGHTROOM) == 0 then
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
    fr:leave_team(self)
    fr:end_fight()
end

-- 不做 die 钩子：教头 2000 血不会在比试窗口内被打散，走标准死亡非本段语义
return npc
