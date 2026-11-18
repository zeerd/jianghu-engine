-- sample_bandit_boss.lua  山寨大当家
-- 示例NPC：展示高级战斗NPC的简单实现，含自动出绝招和定时回血；
-- 另演示 NPC 存盘（can_save）：挑战计数随每次开战累加并跨重启恢复

local npc = {}

function npc.create(self)
    self:set_name("黑风寨主", "hei feng zhai zhu")
    self:set("title", "大当家")

    -- NPC 的 CAN_SAVE 读 dbase 键 can_save（CNpc::query_attr）：置 1 后
    -- self:save_to_local() / self:load_from_local() 才会走 t_npc 表（obj_id = base_name）
    self:set("can_save", 1)

    self:set("gender", "男性")
    self:set("age", 45)
    self:set("long", "身材魁梧，满脸横肉，手持一把九环大刀，气势逼人。")
    self:set("attitude", "aggressive")
    self:set("shen_type", -1)

    self:set("level", 12)
    self:set("combat_exp", 100000)

    self:set("max_hp", 15000)
    self:set("hp", 15000)
    self:set("max_mp", 3000)
    self:set("mp", 3000)
    self:set("mp_factor", 80)

    self:set("str", 35)
    self:set("con", 35)
    self:set("dex", 30)
    self:set("int", 15)

    -- 高级战斗技能
    self:set_skill("sword", 100 + math.random(0, 19))
    self:set_skill("sword_adv", 80 + math.random(0, 19))
    self:set_skill("force", 100 + math.random(0, 19))
    self:set_skill("force_adv", 80 + math.random(0, 19))
    self:set_skill("dodge", 90 + math.random(0, 19))
    self:set_skill("dodge_adv", 70 + math.random(0, 19))
    self:set_skill("parry", 90 + math.random(0, 19))
    self:set_skill("parry_adv", 70 + math.random(0, 19))
    self:set_skill("unarmed", 80 + math.random(0, 19))

    self:skill_map("set", "sword", "sword_adv")
    self:skill_map("set", "force", "force_adv")
    self:skill_map("set", "dodge", "dodge_adv")
    self:skill_map("set", "parry", "parry_adv")
    self:skill_map("set", "unarmed", "unarmed")

    self:skill_prepare("set", "unarmed", "unarmed")

    -- 佩刀归山寨自有（owner/no_get/no_duo，夺不走），取出即装备
    self:wield_ownered_item("sample_sword_adv")

    -- 怀里的赃钱：coin 挂在 NPC 身上没有主人，死后随尸体落地
    self:add_money(300)

    self:set("apply/armor", 60)
    self:set("apply/damage", 40)

    -- 战斗中自动出招概率较高
    self:set("chat_chance_combat", 40)

    -- 定时回血
    self:call_out("do_full", 15)

    -- 恢复存档（挑战计数跨重启就靠这里）：必须放在取剑之后——
    -- restore_data 会整表覆盖 dbase 并清掉 infinity，而 max_encumb 要到
    -- setup() 才设置，提前 Load 会让剑因容量校验装不上；首次无档时 Load
    -- 返回 0、不动 dbase，保留上面 create 的初值。
    self:load_from_local()
end

-- 有玩家进房：记一次挑战、立即存盘、喊话并主动开战。
-- 每次进房恰好触发一次，天然一场一计数；定义 init 后引擎不再代跑基类
-- CChar::init 的延迟 auto_fight——进房即开战取而代之。
function npc.init(self, me)
    if not me:is_user() then
        return
    end
    if self:is_fighting() then
        return
    end
    if me:query("net_dead", "T") ~= 0 then
        return
    end

    self:add("challenge_count", 1)
    -- 即时落库：不依赖关服时机，喊话前计数已持久化
    self:save_to_local()
    self:say(("你已经是第%d个来送死的了！"):format(self:query("challenge_count")))
    -- 必须在 say 之后：开战会把玩家移进战斗房，之后的房间播报他就收不到了
    self:kill_ob(me)
end

-- 定时回血
function npc.do_full(self, param1, param2)
    if not self:query("fight/is_fighting", "T") then
        -- 脱战后回血
        if self:query("hp") < self:query("max_hp") then
            self:set("hp", math.min(self:query("hp") + 500, self:query("max_hp")))
        end
        if self:query("mp") < self:query("max_mp") then
            self:set("mp", math.min(self:query("mp") + 200, self:query("max_mp")))
        end
    else
        -- 战斗中也有概率回血
        if math.random(4) == 1 and self:query("hp") < self:query("max_hp") // 2 then
            self:perform_action("force recover", 0)
        end
    end
    self:call_out("do_full", 15)
end

-- 战斗中自动出绝招
function npc.chat_msg_combat(self)
    local r = math.random(5)
    if r == 1 then
        -- 使用剑气
        self:perform_action("sword qi", 1)
    elseif r == 2 then
        -- 使用狮子吼
        self:perform_action("force roar", 1)
    elseif r == 3 then
        -- 使用自疗术
        if self:query("hp") < self:query("max_hp") // 2 then
            self:perform_action("force heal", 0)
        end
    elseif r == 4 then
        -- 使用连环击
        self:perform_action("unarmed combo", 1)
    elseif r == 5 then
        -- 使用凌空虚渡
        self:perform_action("dodge float", 0)
    end
end

return npc
