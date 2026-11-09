-- sample_heifeng_master.lua  黑风寨总教头
-- 示例NPC：展示「判师改投」——已有门派者来投，先判出原师门（脱离师门 +
-- betrayer 记一次账），再由本角色 recruit 收入黑风寨；判师账要付代价，去原
-- 师门问话「悔过」才消得掉（见 sample_master）。
--
-- 为什么判师走 do_talk 不走 /apprentice：改换门派的闸在命令层
-- （apprentice 直接拒「未经本派师门同意不能改换门派」，收徒钩子轮不到），
-- 所以先问话清掉家门，再由对方 /apprentice——此时 recruit 的 pending 已挂好，
-- 直走成功支完成入伙。

local npc = {}

function npc.create(self)
    self:set_name("黑风寨总教头", "hei feng zhai zong jiao tou")
    self:set("title", "总教头")
    self:set("long", "黑风寨总教头。")
    self:set("attitude", "peaceful")
    self:set("age", 40)

    self:set("str", 20)
    self:set("con", 20)
    self:set("dex", 20)
    self:set("int", 20)
    self:set("max_hp", 1000)
    self:set("hp", 1000)

    -- generation=1 是收徒资格的硬条件（recruit 查 family/generation）；
    -- 已有显式 title，create_family 不会把称号改成「开山祖师」
    self:create_family("黑风寨", 1, "开山祖师")
end

function npc.do_talk(self, me, ask)
    if ask ~= "判师" then
        return self:base_do_talk(me, ask)
    end

    self:say("先与原师门了断，再入黑风寨。", me)

    -- 判师即散功：全清本门所学（本门心法随之被废，删前删后引擎实读报数），
    -- 单删、准备表、映射表、技能列表刷新一并走一遍——判师的代价就是忘光
    local had = me:query_skill("qingyun_xinfa", 1)
    me:delete_skill("qingyun_xinfa")
    me:FlushMySkill()
    tell_object(me, string.format("你已散尽本门武学：青云心法 %d 级已废。", had))

    me:apprentice("release")
    me:del("family")
    me:del("class")
    me:add("betrayer", 1)
    self:command("recruit " .. me:object_id())
    return 1
end

return npc
