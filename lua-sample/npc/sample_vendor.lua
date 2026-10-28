-- sample_vendor.lua  山脚商人
-- 示例NPC：展示商人NPC的简单实现
-- 除买卖外，还演示 NPC 侧的「对话菜单 + 菜单命令分发」：
--   do_talk 在问话提到「交易」时列菜单，其余问话交回 base_do_talk 保持
--   商家询问回绝；菜单命令经 handle_action 路由到 do_get_trade_doc，
--   向玩家发放交易文书（lua-default/item/trade.lua 贸易文书，参考
--   jianghu-dataset/npc/pub2/pub_xiaoer4.lua 的 gettrade）。

local npc = {}
npc.type = "vendor"

function npc.create(self)
    self:set_name("杂货老板", "huo ji")

    self:set("gender", "男性")
    self:set("age", 40)
    self:set("long", "一个精明的商人，在山脚开着一间杂货铺。")
    self:set("attitude", "friendly")
    self:set("no_kill", 1)

    self:set("combat_exp", 5000)

    -- 上架商品
    self:carry_good("sample_sword_basic", 10)
    self:carry_good("sample_sword_adv", 3)
    self:carry_good("sample_book_force", 5)
    self:carry_good("sample_book_sword", 5)
    self:carry_good("sample_heal", 200)
    self:carry_good("sample_hp", 200)
    self:carry_good("sample_poison", 5)
    self:carry_good("sample_teleport", 2)
    self:carry_good("sample_teapot", 5)
    self:carry_good("sample_chess", 20)

    -- 自身装备
    self:carry_object("sample_sword_basic"):wear()
end

-- 对话入口：问话提到「交易」才列菜单；其余问话交回 base_do_talk，
-- 保持本 NPC 对陌生问话的「对方并不愿意理你。」回绝（见 vendor_talk.sce）
function npc.do_talk(self, me, ask)
    if ask ~= "" and not string.find(ask, "交易", 1, true) then
        return self:base_do_talk(me, ask)
    end

    self:AddMenuItem("索取一份交易文书", "$0get_trade_doc $1", me)
    self:SendMenu(me)
    return 1
end

-- 发放交易文书：人手一份，身上已有则拒绝
function npc.do_get_trade_doc(self, me)
    if me:present("trade") then
        return notify_fail("你已经有一份交易文书了，一份足矣。")
    end

    local doc = load_item("trade")
    if not doc then
        return notify_fail("交易文书用完了，改日再来吧。")
    end

    doc:move(me)
    message_vision("$N递给$n一份交易文书。", self, me)
    self:say("拿好了，凭这份文书才好与各地商人做买卖。", me)
    self:SendMenu(me)
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
