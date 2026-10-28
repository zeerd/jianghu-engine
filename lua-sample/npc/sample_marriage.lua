-- sample_marriage.lua  青云城结婚登记员
-- 示例NPC：演示结婚/离婚全流程（简化版，无复杂渲染）。
-- 结婚走「付款领婚姻登记表 → 双方先后签名」两段仪式：
--   登记表是独立示例物品 item/sample_marriage_form.lua（参考同心印的两段式
--   绑定，「和谁结婚」由物品字段 + 房间在场承载）。
-- 离婚走下方菜单确认流程。
-- 引擎没有 /marry 玩家命令，本文件也不要发明 marry_target 之类的玩家侧状态。

local npc = {}
npc.type = "vendor"

function npc.create(self)
    self:set_name("月老", "yue lao")
    self:set("title", "青云城婚姻登记处")
    self:set("gender", "男性")
    self:set("age", 80)
    self:set("per", 20)
    self:set("long", "一位白须飘飘的老者，手里拿着一本泛黄的姻缘簿。")

    self:set("max_hp", 1000)
    self:set("combat_exp", 1000)
    self:set("attitude", "friendly")
    self:set("no_fight", 1)
end

-- 结婚咨询
function npc.do_marry_info(self, me)
    if me:querymap("couple") then
        self:say("你已经结婚了，一生一世一双人。", me)
        self:SendMenu(me)
        return 1
    end

    self:say("想结婚吗？双方自愿，到我这登记即可。", me)
    self:say("费用：100 金。交钱领一张婚姻登记表：", me)
    self:say("你先在表上签名，再把它交给意中人，由对方在你身边签名，即结为夫妻。", me)
    self:SendMenu(me)
    return 1
end

-- 离婚咨询
function npc.do_divorce_info(self, me)
    if not me:querymap("couple") then
        self:say("你还没结婚，何来离婚？", me)
        self:SendMenu(me)
        return 1
    end

    local partner = find_online(me:querystr("couple/id"))
    if not partner then
        self:say("你的伴侣不在线，无法办理离婚。", me)
        self:SendMenu(me)
        return 1
    end

    self:say("离婚乃大事，三思而后行。确定要离婚吗？", me)
    self:AddMenuItem("我决心已下", "$0confirm_divorce $1", me)
    self:AddMenuItem("再考虑考虑", "$0cancel $1", me)
    self:SendMenu(me)
    return 1
end

-- 确认离婚
function npc.do_confirm_divorce(self, me)
    local partner = find_online(me:querystr("couple/id"))
    if not partner then
        self:say("你的伴侣已下线，无法办理。", me)
        self:SendMenu(me)
        return 1
    end

    -- 解除婚姻关系
    me:del("couple")
    me:add("couple_betray", 1)
    partner:del("couple")
    partner:add("couple_betray", 1)

    self:say("姻缘已尽，各自珍重。", me)
    message_vision("$N与$n解除了夫妻关系。", me, partner)
    self:SendMenu(me)
    return 1
end

-- 收钱发婚姻登记表（登记仪式在表上完成，见 item/sample_marriage_form.lua）
function npc.accept_object(self, who, ob)
    if who:querymap("couple") then
        self:say("你已经结婚了。", who)
        self:SendMenu(who)
        return 0
    end

    if ob:querystr("id") == "coin" and ob:query("value") >= 100 then
        local form = load_item("sample_marriage_form")
        if not form then
            self:say("登记表用完了，改日再来吧。", who)
            self:SendMenu(who)
            return 0
        end

        destruct(ob)
        message_vision("$N接过月老递来的一张婚姻登记表。", who)
        form:move(who)
        self:say("先自己签名，再让意中人在你身边签名，登记即成。", who)
        self:SendMenu(who)
        return 1
    end

    return 0
end

function npc.do_talk(self, me, ask)
    self:AddMenuItem("结婚登记", "$0marry_info $1", me)
    self:AddMenuItem("离婚登记", "$0divorce_info $1", me)
    self:SendMenu(me)
    return 1
end

function npc.handle_action(self, comm, me, arg)
    if comm == "marry_info" then
        return npc.do_marry_info(self, me)
    end
    if comm == "divorce_info" then
        return npc.do_divorce_info(self, me)
    end
    if comm == "confirm_divorce" then
        return npc.do_confirm_divorce(self, me)
    end
    if comm == "cancel" then
        return 1
    end

    return self:base_handle_action(comm, me, arg)
end

return npc
