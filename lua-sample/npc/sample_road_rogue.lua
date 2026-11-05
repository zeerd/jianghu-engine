-- sample_road_rogue.lua  山道强人
-- 示例NPC：展示「记仇 / 切磋」这条正常战斗逻辑在脚本侧的配合。
--
-- 进战斗的两条正常入口在引擎里泾渭分明（src\cmd\CmdStandardCombat.cpp）：
--   /fight 切磋 → CNpc::accept_fight → do_bishi 比试房，**全程不碰杀意名单**；
--   /kill  记仇 → cmd_hit_ob + 双向 set_killer，双方进名单（源码注释即「记仇的」）。
-- 名单记的是跨会面的追杀关系：`is_killing` 读它、`remove_all_killer` 撤它。
--
-- 本脚本不主动宣战（进房即宣死战是「表达恶意」的特殊用法，不是本意），按对方的
-- 话头分岔——**出言不逊才记账，客气请教才当场还手**：
--   do_talk「挑衅」  第一次：set_killer 记下这笔账。注意 set_killer **并不开打**，
--                    它只把人写进追杀名单（下次进房由 CChar::init 的 hatred 分支
--                    自动开战），所以同一句挑衅可以反复问，话一直说得下去；
--                    名单已在 → 报出「新账旧账一起算」（is_killing 真支，读的是
--                    强人自己那一份名单，与 duanjia_sword.lua 的 target:is_killing(me) 同向）
--   do_talk「讨教」  点到为止：fight_ob 还手但不记账——这一句才真正开战，
--                    排在所有问话之后，开战后 talk 被引擎挡掉也不影响
--   accept_object    收受贿赂：钱到气消，双方 remove_all_killer 销账
--   die              死亡接管：killer_reward 结算击杀奖励 → MakeCorpse 造尸体 → 自毁
--
-- 战斗中 talk 被引擎挡掉（CmdUserSocial.cpp 的 is_fighting 分支），所以几句话都
-- 排在开战之前；贿赂同理排在开战之前——非战斗态下 give 不走 30 AP 那道门。
--
-- 暗器追击不在本文件：他每回合出手后的补击挂在 data/hooks.lua 的 hooks.fight
-- （按 hidden_weapon 标记过滤），补击伤害由 damge_to_string 格式化后私发对手。

local npc = {}

function npc.create(self)
    self:set_name("山道强人", "shan dao qiang ren")
    self:set("gender", "男性")
    self:set("age", 30)
    self:set("long", "一个斜挎包袱的汉子，眼神总往行人的钱袋上飘。")
    -- 拦路强人：记仇但**不主动寻衅**。attitude 用 killer 而非 aggressive——
    -- aggressive 会在 CChar::init 里给每个进房的玩家排一颗 5~10 秒后引爆的
    -- 自动开战（start_aggressive 直接 kill_ob），会把下面几句话的窗口炸掉；
    -- killer 与 aggressive 在 CNpc::accept_fight 里同支（照样回「哼！出招吧！」），
    -- 却不进那条自动寻衅分支
    self:set("attitude", "killer")
    self:set("shen_type", -1)

    -- 血量按「从开战打到死」算：玩家约四秒一击、每击二十多点，给厚了是白等，
    -- 给薄了撑不到断言收齐；比试房的 50% 判负线要四击才够得到，而场1 只打
    -- 三秒、实测一击，够不着。战力压低是因为这里演的是状态链，输赢不在设计内
    self:set("level", 1)
    self:set("combat_exp", 100)
    self:set("max_hp", 200)
    self:set("hp", 200)
    self:set("str", 10)
    self:set("con", 10)
    self:set("dex", 10)
    self:set("int", 10)

    -- 暗器：hooks.fight 的追击只对带 hidden_weapon 标记的角色生效，
    -- 棋子（skill_type=throwing）背在身上即可，不必装备到手上
    self:set("hidden_weapon", 1)
    self:carry_object("sample_chess")
    self:set_skill("throwing", 30)

    -- 固定击杀经验：拦路的身上带着官府悬赏，谁杀都能领到——不走 nk_gain 的
    -- 历练折算（见 data/hooks.lua 的 kill_bonus）
    self:set("nk_bonus", 100)
end

-- 问话：按话头分岔。记账的记账、还手的还手，两支都在战斗外，写下去真生效。
-- 名单方向是**强人自己那一份**——挑衅记的是「我记着你」，所以读 self 对 me，
-- 与 duanjia_sword.lua 的 target:is_killing(me) 同向。
function npc.do_talk(self, me, ask)
    if ask == "挑衅" then
        if self:is_killing(me) then
            -- 我早把你列进追杀名单了：新账旧账一起算
            self:say("我早记下你了，正好新账旧账一起算。", me)
        else
            -- 第一次出言不逊：记下这笔账（写）。不动手——记的是跨会面的追杀，
            -- 下次进房由 hatred 分支自动开战
            self:set_killer(me)
            self:say("好，这笔账我记下了。", me)
        end
        return 1
    end

    if ask == "讨教" then
        -- 名单空着也只是切磋：还手但不记账（写）
        self:say("无仇无怨，点到为止——接招。", me)
        self:fight_ob(me)
        return 1
    end

    return self:base_do_talk(me, ask)
end

-- 收受贿赂：钱到气消，双方名单一起销（写）。只认钱，别的礼不受理
function npc.accept_object(self, who, ob)
    if ob:querystr("id") ~= "coin" then
        return 0
    end

    self:remove_all_killer()
    who:remove_all_killer()
    self:say("杀意已散，恩怨两清。", who)
    return 1
end

-- 死亡接管：奖励与尸体都由本脚本装配，顺序与引擎标准死亡一致——
-- 先结算击杀奖励，再造尸体，最后自毁
function npc.die(self)
    local killer = find_object(self:query_temp("last_damage_from"), 1)

    message_vision("$N闷哼一声，仰面栽倒在地。", self)

    if killer then
        killer_reward(killer, self)
    end
    MakeCorpse(self, killer)

    destruct(self)
end

return npc
