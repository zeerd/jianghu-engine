-- hooks.lua — sample 世界（script_root = lua-sample）的 C++ 挂点实现
--
-- 加载规则：开机按 script_root → default_script_root 逐级回退，先命中者生效。
-- 本文件位于 script_root，覆盖 lua-default/data/hooks.lua 参考脚本；
-- 未在此实现的挂点仍按“缺席 = C++ 默认逻辑”生效。
-- 通用语义与各挂点签名见 lua-default/data/hooks.lua 的注释。
--
-- 出手准入（hooks.combat_rule attack_gate）：本世界不设新手门槛——能否对他人
--   动手属世界观策略，由数据集声明；引擎只在 CChar::cmd_hit_ob 询问本钩子，
--   各世界按「hooks.lua 整文件先命中者生效」各自给规则。sample 世界由此放行
--   任何存活对象相互出手；出手后果（PK 罪恶值、城市守卫、死亡惩罚）仍由引擎
--   与数据集照常结算。要给本世界设门槛，在本文件实现 hooks.combat_rule 即可
--   （写法见 lua-default/data/hooks.lua）。
require("data.rank").install(hooks)
require("data.quest").install(hooks)
-- 天下大考：answer 频道接管（出题 / 判分 / 奖励，见 data/exam.lua）
require("data.exam").install(hooks)
-- 开盲盒记档：建表语句须在 require 期登记（建表由开机 prepare_tables 统一
-- 执行，晚于它登记表就永远建不出来；见 data/box_draw_log.lua）
require("data.box_draw_log")

-- 新角色起步本金（金）
-- 取值偏大：住宅区地价随住户数走阶梯（见 resident_base HOUSE_PRICES，最低档
-- 亦达 50 万），而首个置业者登记后 policy 地价会被 set_house 的 Load 覆盖、
-- 后续置业者回落到阶梯价——起步本金需足以支付，避免 E2E 第二次置业「现金不足」。
local START_MONEY = 10000000

-- hooks.setup_human(me)
--   调用点：C++ setup_human 末尾（属性/气血/分支/体重之后）。注册与每次登录
--   都会走到，故以 dbase 的 start_money 标记 gating，只在首次发放一次。
--   发放起步本金：新角色靠它购物、置业、存款，自立门户。
--   只作用于玩家：青云子、老李头等 NPC 同为人类 race，也会走到此钩子。
function hooks.setup_human(me)
    if not is_user(me) then
        return
    end
    if me:query("start_money") ~= 0 then
        return
    end
    pay_player(me, START_MONEY)
    me:set("start_money", 1)
end

-- hooks.kill_bonus(killer, victim, reward)
--   调用点：CReward::handle_npc_kill_reward，击杀 NPC 的经验按 level*4+100
--   封顶之后、入账之前。每次玩家击杀 NPC 都会被咨询（reward 可能为 0），
--   脚本返回的固定奖励不受 nk_gain 上游因素（组队/晕倒归属/经验折算）影响。
--   返回 {reward=N} = 以 N 入账；nil / 缺席 = 按 reward 入账；
--   咨询后仍 <=0 的奖励不入账、不发提示。
function hooks.kill_bonus(killer, victim, reward)
    -- 练级怪（青蛇）：击杀固定奖励 10000 经验（房间见 room/sample_mountain_top.lua）
    if victim:query("xp_bonus") == 1 then
        return { reward = 10000 }
    end
    -- 固定击杀经验（nk_bonus）：入账值原本要过 nk_gain 的历练折算——对手历练
    -- 远高于这类杂鱼时折出来就是 0，杀了等于白杀；写死在本挂点上才折不掉
    local bonus = victim:query("nk_bonus")
    if bonus ~= 0 then
        return { reward = bonus }
    end
    return nil
end

-- hooks.fight(me, victim)
--   调用点：CCombatd::fight 每次常规出手结算完之后，攻守双方各走一次。
--   这里做「追加攻击」：带 hidden_weapon 标记的角色（山道强人）在自己那一
--   回合补一记棋子——取身上第一件暗器发动一次独立结算。
--   flag 走 TYPE_HIT（必中档）：这一档把对手的闪避值与招架值都压到 1，
--   暗器是贴身抖出去的，中不中只看出手的人，不给对手凭身法让开的余地。
function hooks.fight(me, victim)
    if me:query("hidden_weapon") ~= 1 then
        return
    end

    local weapon = get_first_throwing_weapon(me)
    if not weapon then
        return
    end

    combat_attack_weapon(me, victim, weapon, TYPE_HIT)
end

-- hooks.post_buy(vendor, buyer, goods)
--   调用点：CVendor::do_buy 成交之后（引擎只给「买下了」回执，成交后想对
--   买卖双方说的话在此给）。盲盒「拆开之前不退不换」是它独有的规矩，故按
--   goods.basename 收窄到盲盒，别的货位不发这句——message_vision_self 只送
--   买卖双方、不进房间广播。
function hooks.post_buy(vendor, buyer, goods)
    if goods.basename ~= "sample_blind_box" then
        return
    end

    message_vision_self("$N冲$n笑道：盲盒拆开之前不退不换，想清楚再开。", vendor, buyer)
end

-- hooks.calculate_damage(me, victim, damage)
--   调用点：CAttack 伤害算完、落到身上之前，damage 就是这一击的实打数值。
--   这里把打在山道强人身上的每一击格式化后私发给出手方——他自己的攻击几乎
--   不会落空，文案稳定；返回 nil 即不改写伤害。
function hooks.calculate_damage(me, victim, damage)
    if victim:query("hidden_weapon") ~= 1 then
        return nil
    end

    tell_object(me, "这一击" .. damge_to_string(damage))
    return nil
end
