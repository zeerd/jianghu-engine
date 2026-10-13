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

    self:call_out("do_refresh", 1)
end

function npc.ask_master(self, player)
    self:say("半山腰住着一位隐世高手，据说可以教你武功。")
    if player:query("reward/exp") < 1 then
        player:add("combat_exp", 50000)
        player:set("reward/exp", 1)
    end
end

function npc.do_refresh(self, param1, param2)
    local map = load_room("sample_mountain_top")
    if map:query("random/tree") == 0 then
        if random(10) == 1 then
            local box = map:Add_Box()
            box:add_object("sample_xp", 1)
            box:set_name("一个奇怪的树", "tree")
            map:set("random/tree", 1)
        end
    else
        self:call_out("do_refresh", 5)
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

return npc
