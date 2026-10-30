-- sample_wooden_dummy.lua  半山腰练功木人
-- 示例NPC：展示镜像NPC——脚本自己 require 镜像基模块 npc/mirror_base.lua
-- 并在文件末尾 install 进本脚本表（缺省钩子 die/chat_msg/chat_msg_combat
-- 随装配补齐），镜像不是引擎类型，就是普通脚本 NPC 加一次模块装配，
-- 顺带讲清「NPC 问答该走哪条入口」：标准做法是注册询问，本文件故意改用 do_talk，
-- 两者的分工与参考对象分列如下。
--
-- 一、标准做法：问答走 set_inquiry / set_inquiry_func，不写 do_talk
--   self:set_inquiry("山", "预置回答")            -- 命中即答
--   self:set_inquiry_func("师父", "ask_master")   -- 命中即调，回调自己作答
--   范本：lua-sample/npc/sample_guide.lua（老李头：三条预置 + 两条回调）。
--   引擎侧统一走 src/lua/LuaNpc.inl 的 do_lua_inquiry，只有四种去向：
--     · 命中预置回答 → say(answer, me) 私发给问话人 + 弹菜单；
--     · 命中回调     → 回调返回字符串，引擎替你 say + 弹菜单；不返回字符串则
--                       引擎一个字都不发、也不弹菜单，回执全由回调自己给（老李头
--                       的回调即自己 say——不带 me，房间播报——且不返回值）；
--     · 问错但注册过 → 把全部话题列成菜单发给玩家，等于免费告诉对方能问什么；
--     · 一条都没注册 → notify_fail「对方并不愿意理你。」（仅问话人可见）。
--
-- 二、这里改用 do_talk 想表达什么：木人的「对练」是动作，不是问答
--   1. 正事是 make_mirror——先改自己的技能/属性、再把名字改回、最后私发一行，
--      动作时序摆在对话入口里最直白（回调里也写得出来，但那是借问答通道下发
--      动作，语义就偏了）；
--   2. 要「私发一行 + 不弹菜单 + 返回已处理」的回执：do_talk 返回什么就是什么
--      （LuaNpc.inl），而 inquiry 的回答与菜单绑死——要么一起给，要么都不给；
--   3. 想自己决定问错时的去向：本文件一旦注册过「对练」，inquiry 就再也给不出
--      notify_fail，只会走「列菜单」分支；改用 do_talk 才能把陌生问话明确回退给
--      base_do_talk → notify_fail；
--   4. 顺带吃下空 ask（裸 /ask）：查表查不到 ""，inquiry 会落进列菜单分支。
--   「入口归脚本、其余回退基类」是样本既有惯例，同形参考
--   lua-sample/npc/monster_yongren.lua 的 handle_action：只认自己的子命令，
--   末行 return self:base_handle_action(...) 把其余交回引擎。
--   另附一条接口事实：通用对话钩子 hooks.pre_talk 只在 base_do_talk 里咨询，
--   故对练分支由本脚本全权负责，其余问话回退时任务钩子仍有机会接管。
--
-- 三、对练（/t 练功木人 + /ask 对练）做了什么
--   require 来的镜像基模块 make_mirror(self, me, diff) 把 me 的技能、技能映射、
--   激发、先天属性、气血内力与历练等级整套按 diff / 10 抄到木人身上——
--   木人当场变成「八成的你」，可以拿来回合对练（简版基模块不配武器，
--   原版会照抄来的技击技能配一把「木刺」）。
--   抄的时候有两个坑，写在这儿免得改脚本的人踩：
--     1. make_mirror 不命名，脚本必须自行 set_name 设置，
--        否则 /t 练功木人 从此认不出它；
--     2. 镜像基模块的 chat_msg 每次有 1/10 概率「遁走自毁」、die 是「惨叫一声，
--        化为灰烬」，都是镜像怪的台词：本脚本不设 chat_chance（心跳派发不到
--        chat_msg），并自己接管 die（见文件末尾）——install 放在文件末尾，
--        脚本自己定义的钩子优先、基模块只补缺。
--
-- 四、平时与死后
--   平时它只是一截挨打的木头：attitude 不是 aggressive，CChar::init 的自动仇恨
--   分支不会被触发，谁来打它都只是在打木头。
--   被打散即自毁，房间 reset 会按 add_npc 的 base_name 重新载入一份，抄来的状态
--   随之清零；不走 die_base 意味着不掉尸体、也不结算 NK 经验。

local npc = {}

-- 镜像行为的来源：本脚本自己装配基模块——缺省钩子（die/chat_msg/
-- chat_msg_combat）install 进脚本表，make_mirror 经模块引用调用
-- （self: 认不到脚本函数）
local mirror_base = require("npc.mirror_base")

-- 对练倍率：make_mirror 的 diff 取 1~20，实际倍率 = diff / 10
local SPARRING_DIFF = 8
local SPARRING_DIFF_TEXT = "八成"

-- 还没跟人对过招时的平庸身手，只够挨打
local BASE_SKILL_LEVEL = 10

function npc.create(self)
    self:set_name("练功木人", "mu ren")
    self:set(
        "long",
        "一个半人高的练功木人，关节处裹着旧麻布，身上遍布刀痕拳印。木胸上"
            .. "刻着一行小字：「与我对练」——向它 /ask 对练，它便照你的路数"
            .. "拆招。"
    )

    self:set("attitude", "peaceful")
    self:set("age", 1)
    self:set("level", 1)
    self:set("combat_exp", 100)

    self:set("max_hp", 300)
    self:set("hp", 300)

    self:set("str", 10)
    self:set("con", 10)
    self:set("dex", 10)
    self:set("int", 10)

    self:set_skill("force", BASE_SKILL_LEVEL)
    self:set_skill("unarmed", BASE_SKILL_LEVEL)
    self:set_skill("parry", BASE_SKILL_LEVEL)
    self:set_skill("dodge", BASE_SKILL_LEVEL)

    self:map_skill("force", "force")
    self:map_skill("unarmed", "unarmed")
    self:map_skill("parry", "parry")
    self:map_skill("dodge", "dodge")
end

-- 对话入口（为什么在这里而不用 set_inquiry，见文件头第一、二节）：
--   命中对练问话 → 照挑战者抄一次、私发一行回执、return 1 = 本次已处理：
--     指令层据此跳过失败回执，玩家只看到这一行；返回 0 则会连 talk() 开头
--     预置的「对方并不愿意理你。」一起发出来（CmdHandle 的 NOTIFY_FAIL 分支）；
--   其余问话 → 回退 base_do_talk：本脚本没注册询问，do_lua_inquiry 回
--     notify_fail「对方并不愿意理你。」；任务钩子 hooks.pre_talk 也在此被咨询。
function npc.do_talk(self, me, ask)
    if ask ~= "" and ask ~= "对练" and ask ~= "练功" then
        -- 未命中关键词：交回引擎通用流程（标准做法的去向，见文件头第一节）
        return self:base_do_talk(me, ask)
    end

    mirror_base.make_mirror(self, me, SPARRING_DIFF)
    -- make_mirror 不设置名字，脚本自行 set_name
    self:set_name("练功木人", "mu ren")

    -- say(msg, me) 私发：只有问话人收得到，同房其他账号看不到
    self:say(
        "木人的关节咔哒作响，照着你的起手式摆开了门户，抄去了你"
            .. SPARRING_DIFF_TEXT
            .. "的本事——动手吧。",
        me
    )
    return 1
end

-- 死亡：被打散就散成一地木头并自毁（不走 die_base：不掉尸体、不结算 NK 经验）
function npc.die(self)
    message_vision("$N的关节一松，噼里啪啦散成了一堆木头。", self)
    destruct(self)
end

-- 装配镜像缺省钩子：必须放在全部脚本定义之后——install 只补缺失项，
-- 自己定义过的（如上面的 die）不被覆盖
mirror_base.install(npc)

return npc
