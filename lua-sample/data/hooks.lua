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
-- 世界设定：一笔够用的盘缠——日常买药、置产首付（住宅区走 resident_base
-- 缺省阶梯地价，单人间折半约 25 万）后仍有余量。
local START_MONEY = 600000

-- hooks.setup_human(me)
--   调用点：C++ setup_human 末尾（属性/气血/分支/体重之后）。注册与每次登录
--   都会走到，故以 dbase 的 start_money 标记 gating，只在首次发放一次。
--   发放起步本金：新角色靠它购物、置业、存款，自立门户。
--   只作用于玩家：青云子、老李头等 NPC 同为人类 race，也会走到此钩子。
function hooks.setup_human(me)
    if not me:is_user() then
        return
    end
    if me:query("start_money") ~= 0 then
        return
    end
    pay_money("give", me, START_MONEY)
    me:set("start_money", 1)
end

-- hooks.kill_bonus(killer, victim, reward)
--   调用点：CReward::handle_npc_kill_reward，击杀 NPC 的经验按 level*4+100
--   封顶之后、入账之前。每次玩家击杀 NPC 都会被咨询（reward 可能为 0），
--   脚本返回的固定奖励不受 nk_gain 上游因素（组队/晕倒归属/经验折算）影响。
--   返回 {reward=N} = 以 N 入账；nil / 缺席 = 按 reward 入账；
--   咨询后仍 <=0 的奖励不入账、不发提示。
function hooks.kill_bonus(killer, victim, reward)
    -- 固定击杀经验（nk_bonus）：入账值原本要过 nk_gain 的历练折算——对手历练
    -- 远高于这类杂鱼时折出来就是 0，杀了等于白杀；写死在本挂点上才折不掉。
    -- 练级怪青蛇 nk_bonus=10000，山道强人 nk_bonus=100，其余 NPC 缺席走常规入账
    local bonus = victim:query("nk_bonus")
    if bonus ~= 0 then
        return { reward = bonus }
    end
    return nil
end

-- hooks.fight(me, victim)
--   调用点：CCombatd::fight 每次常规出手结算完之后，攻守双方各走一次。
--   这里做两类「追加攻击」：
--   1. 暗器补击（山道强人）：带 hidden_weapon 标记的角色在自己那一回合
--      补一记棋子——取身上第一件暗器发动一次独立结算。flag 走 TYPE_HIT
--      （必中档）：这一档把对手的闪避值与招架值都压到 1，暗器是贴身抖出
--      去的，中不中只看出手的人，不给对手凭身法让开的余地。
--   2. 左手连击：左手是标记了 can_combo 的轻便短兵（匕首）时快手再补一记
--      ——action_flag 置位期间战报带「紧跟着」，与 dataset 的连击同款。
function hooks.fight(me, victim)
    if me:query("hidden_weapon") == 1 then
        local weapon = get_first_throwing_weapon(me)
        if weapon then
            combat_attack("weapon", me, victim, weapon, TYPE_HIT)
        end
    end

    local left = me:query_left_weapon()
    if left and left:query("can_combo") ~= 0 then
        me:set("action_flag", 1, "T")
        tell_object(me, "你左手短兵疾递，连环刺出！")
        combat_attack("weapon", me, victim, left, TYPE_QUICK)
        me:set("action_flag", 0, "T")
    end
end

-- hooks.pre_buy(vendor, buyer, goods)
--   调用点：CVendor::do_buy 成交前，goods.price 是引擎算好的基准价。
--   商人看脸（势利眼的买卖人）：以 18 分长相为基准价——越好看越便宜、
--   越丑越贵，每高 1 分打 1% 折、每低 1 分加价 1%。钳制在五折~加价五成
--   之间，防极端容貌把成交价打出界（负价等于倒贴钱）。
--   返回 {price=N} 改写成交价；返回 nil 按基准价成交。
function hooks.pre_buy(vendor, buyer, goods)
    -- 18 − 相貌：好看为负（减价），丑为正（加价）
    local factor = 118 - buyer:attr("per")
    if factor > 150 then
        factor = 150
    elseif factor < 50 then
        factor = 50
    end
    -- 整除会把低价商品（如 1 金的蛇胆）折到 0——pay_money("take", …, 0) 判支付失败，
    -- 交易永远不成交（实测：per≥18 时 1×99//100=0 → 穷光蛋），
    -- 故成交价下限保底 1 金
    local price = goods.price * factor // 100
    if price < 1 then
        price = 1
    end
    return { price = price }
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

    message("vision_self", "$N冲$n笑道：盲盒拆开之前不退不换，想清楚再开。", vendor, buyer)
end

-- hooks.calculate_damage(me, victim, damage)
--   调用点：CAttack 伤害算完、落到身上之前，damage 就是这一击的实打数值。
--   这里把打在山道强人身上的每一击格式化后私发给出手方——他自己的攻击几乎
--   不会落空，文案稳定；最后一律原样回传 damage：回传数值即走「改写」通道
--   （引擎以回传值落刀，原样回传表示不增不减），返回 nil 则交还引擎原值。
function hooks.calculate_damage(me, victim, damage)
    if victim:query("hidden_weapon") ~= 1 then
        return nil
    end

    tell_object(me, "这一击" .. damge_to_string(damage))
    return damage
end

-- hooks.can_prepare_combine(me, skill_a, skill_b, valid)
--   调用点：/prepare 空手组合的判定（两处）。valid 是既有 valid_combine 的
--   结果，本世界照实回传、不改判——要在此否决组合，返回 false 即可。
--   如果有本来不符合组合条件的要加特例，也要把条件写在这里，然后满足条件的返回 true。
function hooks.can_prepare_combine(me, skill_a, skill_b, valid)
    return valid
end
