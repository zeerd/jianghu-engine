-- sample_vendor.lua  山脚商人
-- 示例NPC：展示商人NPC的简单实现
-- 除买卖外，还演示 NPC 侧的「对话菜单 + 菜单命令分发」：
--   do_talk 在问话提到「交易」时列菜单，其余问话交回 base_do_talk 保持
--   商家询问回绝；菜单命令经 handle_action 路由到 do_get_trade_doc，
--   向玩家发放交易文书（lua-default/item/trade.lua 贸易文书）。

local npc = {}
npc.type = "vendor"

function npc.create(self)
    self:set_name("杂货老板", "huo ji")

    self:set("gender", "男性")
    self:set("age", 40)
    self:set("long", "一个精明的商人，在山脚开着一间杂货铺。")
    self:set("attitude", "friendly")

    self:set("combat_exp", 5000)

    -- 上架商品
    self:carry_good("sample_sword_basic", 10)
    self:carry_good("sample_sword_adv", 3)
    self:carry_good("sample_book_force", 5)
    self:carry_good("sample_book_sword", 5)
    self:carry_good("sample_heal", 200)
    self:carry_good("sample_hp", 200)
    self:carry_good("sample_shedan", 5)
    self:carry_good("sample_teleport", 2)
    self:carry_good("sample_teapot", 5)
    self:carry_good("sample_chess", 20)
    self:carry_good("key", 5)
    self:carry_good("sample_baozi", 20)
    self:carry_good("sample_blind_box", 1000)
    -- 烈酒：出厂即满的酒精饮料，饮下挂 condition/drunk（见 item/sample_jiu.lua）
    self:carry_good("sample_jiu", 5)
    -- 解药：蛇胆之毒的两样解法——解毒丸清全身（见 item/sample_antidote.lua）、
    -- 雄黄专解蛇毒（见 item/sample_xionghuang.lua）。上架固定压在队尾，
    -- 前面各商品的 /buy 序号不随后续加货挪动
    self:carry_good("sample_antidote", 10)
    self:carry_good("sample_xionghuang", 10)
    -- 左手双持：匕首轻便可连发（can_combo）、短剑势沉攻击高
    -- （见 item/sample_dagger.lua、item/sample_short_sword.lua）
    self:carry_good("sample_dagger", 5)
    self:carry_good("sample_short_sword", 3)
    -- 穿戴类：帽/甲/鞋/项链各落一个装备槽，靠 apply/* 给穿戴者加临时属性
    -- （见 item/sample_hat.lua 等）。上架压在队尾，前面各商品的 /buy 序号
    -- 不随后续加货挪动
    self:carry_good("sample_hat", 5)
    self:carry_good("sample_armor", 5)
    self:carry_good("sample_boots", 5)
    self:carry_good("sample_necklace", 5)

    -- 自身装备
    self:carry_object("sample_sword_basic"):wear()
end

-- 对话入口：问话提到「交易」才列菜单；其余问话交回 base_do_talk，
-- 保持本 NPC 对陌生问话的「对方并不愿意理你。」回绝
function npc.do_talk(self, me, ask)
    if ask ~= "" and not string.find(ask, "交易", 1, true) then
        return self:base_do_talk(me, ask)
    end

    self:add_menu_item("索取一份交易文书", "$0get_trade_doc $1", me)
    self:send_menu(me)
    return 1
end

-- 发放交易文书：人手一份，身上已有则拒绝
function npc.do_get_trade_doc(self, me)
    if me:present("id", "trade") then
        return notify_fail("你已经有一份交易文书了，一份足矣。")
    end

    local doc = load("item", "trade")
    if not doc then
        return notify_fail("交易文书用完了，改日再来吧。")
    end

    doc:move(me)
    message("vision", "$N递给$n一份交易文书。", self, me)
    self:say("拿好了，凭这份文书才好与各地商人做买卖。", me)
    self:send_menu(me)
    return 1
end

function npc.handle_action(self, comm, me, arg)
    if comm == "get_trade_doc" then
        return npc.do_get_trade_doc(self, me)
    end

    -- 未命中脚本命令必须显式回退 C++ 基类（CVendor 的 list/buy/sell），
    -- 否则脚本返回 0 会吞掉交易命令，客户端报「未知的命令」
    return self:base_handle_action(comm, me, arg)
end

return npc
