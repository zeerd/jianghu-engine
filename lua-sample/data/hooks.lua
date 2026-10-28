-- hooks.lua — sample 世界（script_root = lua-sample）的 C++ 挂点实现
--
-- 加载规则：开机按 script_root → default_script_root 逐级回退，先命中者生效。
-- 本文件位于 script_root，覆盖 lua-default/data/hooks.lua 参考脚本；
-- 未在此实现的挂点仍按“缺席 = C++ 默认逻辑”生效。
-- 通用语义与各挂点签名见 lua-default/data/hooks.lua 的注释。
require("data.rank").install(hooks)
require("data.quest").install(hooks)

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
    -- 练级山鸡：击杀固定奖励 10000 经验（房间见 room/sample_mountain_top.lua）
    if victim:query("xp_bonus") == 1 then
        return { reward = 10000 }
    end
    return nil
end
