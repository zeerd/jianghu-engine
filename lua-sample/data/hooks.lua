-- hooks.lua — sample 世界（script_root = lua-sample）的 C++ 挂点实现
--
-- 加载规则：开机按 script_root → default_script_root 逐级回退，先命中者生效。
-- 本文件位于 script_root，覆盖 lua-default/data/hooks.lua 参考脚本；
-- 未在此实现的挂点仍按“缺席 = C++ 默认逻辑”生效。
-- 通用语义与各挂点签名见 lua-default/data/hooks.lua 的注释。
require("data.rank").install(hooks)
require("data.quest").install(hooks)

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
