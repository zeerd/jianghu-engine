-- sample_guide.lua  山脚向导
-- 示例NPC：展示聊天NPC的简单实现

local npc = {}

function npc.create(self)
    self:set_name("老李头", "lao li")
    self:set("title", "山脚向导")

    self:set("gender", "男性")
    self:set("age", 55)
    self:set("long", "一个慈祥的老者，常年在山脚为路人指路。")
    self:set("attitude", "friendly")

    self:set("combat_exp", 1000)

    -- 随机聊天概率(%)，同时阻止心跳关闭
    self:set("chat_chance", 5)

    self:set_inquiry("山", "这座山叫青云山，山顶有一伙山贼盘踞，你要小心。")
    self:set_inquiry(
        "山寨",
        "山顶的山寨里有一群山贼，他们经常下山抢劫。你要是有本事，可以去教训他们。"
    )
    self:set_inquiry("商人", "山脚的杂货铺里有个商人，你可以在那里买到武器和药品。")
    self:set_inquiry_func("师父", "ask_master")
    self:set_inquiry_func("排行榜", "ask_rank")

    self:call_out("do_refresh", 1)
end

function npc.ask_master(self, player)
    self:say("半山腰住着一位隐世高手，据说可以教你武功。")
    if player:query("reward/exp") < 1 then
        player:add("combat_exp", 50000)
        player:set("reward/exp", 1)
    end
end

-- 排行榜（演示 paihangbang 全局绑定）：查询历练达标的角色，每条结果经
-- handle_action("update") 回灌本 NPC 播报；查询成功但无人达标时按 0 条播报。
function npc.ask_rank(self, player)
    self:set("rank/rows", 0)
    if paihangbang(self) == 0 then
        self:say("排行榜一时查询不出来，回头再试吧。")
        return
    end
    if self:query("rank/rows") == 0 then
        self:say("排行榜上还没有人达到上榜资格，你可要加把劲。")
    end
end

-- paihangbang 的行消费：comm="update"，
-- arg 为 "<名次> <id> <姓名> <门派> <历练>"
function npc.handle_action(self, me, comm, arg)
    if comm ~= "update" then
        return 0
    end
    self:set("rank/rows", self:query("rank/rows") + 1)
    self:say("排行榜条目：" .. arg)
    return 1
end

-- 随机树箱（概率性事件的演示）：在山顶按 1/10 概率刷出一个装着经验果的
-- 箱子；未中则 5 秒后再掷，直到刷出为止，刷出后停止重排。
-- 注意：房间在 g_room_list 中按「门所引用的 id」索引——本例各房间的门都以
-- 显示名互相引用（见 add_door 的第一个参数），山顶对应的 id 是"青云山山顶"，
-- 而非脚本文件名"sample_mountain_top"。用后者会拿到另一个房间实例，
-- 箱子看不见。
function npc.do_refresh(self, param1, param2)
    local map = load_room("青云山山顶")
    if map:query("random/tree") == 0 then
        if random(10) == 1 then
            local box = map:Add_Box()
            box:add_object("sample_xp", 1)
            box:set_name("一个奇怪的树", "tree")
            map:set("random/tree", 1)
        else
            -- 未命中：5 秒后再掷一次，直到树箱出现
            self:call_out("do_refresh", 5)
        end
    end
end

function npc.chat_msg(self)
    local r = random(6)
    if r == 0 then
        return "哎，最近山上的贼人又多了起来，过往的商队都绕道走了。"
    elseif r == 1 then
        return "年轻人，上山可要小心啊，那些山贼可不是好惹的。"
    elseif r == 2 then
        return "听说半山腰的那位高手以前也是江湖上赫赫有名的人物呢。"
    elseif r == 3 then
        return "山脚的杂货铺老板人不错，你可以在那里备些药品再上山。"
    elseif r == 4 then
        return "这青云山以前可是个好地方，自从来了那伙山贼就不太平了。"
    else
        return "老头子我在这山脚下住了大半辈子，什么风浪没见过。"
    end
end

-- 观察钩子：打量玩家气色（演示 eff_status_msg 全局绑定，按气血比例返回
-- 状态描述）。脚本定义 do_look 后即完全接管观察，不再走 C++ 基座。
function npc.do_look(self, me)
    self:say(self:querystr("long"), me)
    local ratio = 100
    local max_hp = me:query("max_hp")
    if max_hp > 0 then
        ratio = math.floor(me:query("eff_hp") * 100 / max_hp)
    end
    self:say("老李头打量了你一番：" .. eff_status_msg(ratio), me)
    return 1
end

-- 说话反应：玩家定向 say 时作揖回礼（演示 do_emote 全局绑定，
-- 动词表见 lua-default/data/emote.lua）
function npc.relay_say(self, me, msg)
    do_emote(self, me, "bow", 0)
end

return npc
