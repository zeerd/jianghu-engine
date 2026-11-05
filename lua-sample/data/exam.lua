-- exam.lua — 天下大考：用 hooks.do_channel 写一个频道接管的最小示例。
--
-- 引擎不认任何「考试」类型——出题、判分、奖励全是数据集逻辑，挂在通用钩子
-- hooks.do_channel 上（签名与返回值契约见 lua-default/data/hooks.lua）：
--   返回 nil  = 交还内置频道分支（其余频道的回显与渲染不受影响）；
--   返回数组  = 接管本次发言，元素即逐行广播的文案。
-- answer 频道自身的配置（msg_speak/newbie 等）在 data/channel_config.lua，
-- 本模块只管「怎么判、奖什么」。
--
-- 对象参数是 userdata（可调用全部绑定方法），字符串为 GBK 编码。
-- 当前题记在角色临时 dbase（set_temp），不随存档走——考试是当场的事。

local M = {}

-- 答对一题的历练奖励
local REWARD = 1000

-- 题库（作答须与答案逐字一致，GBK 字符串直接比较）
local QUESTIONS = {
    { q = "「床前明月光」出自哪位诗人之手？", a = "李白" },
    { q = "一年共有多少个月？", a = "12" },
    { q = "《笑傲江湖》的男主角是谁？", a = "令狐冲" },
}

function M.install(hooks)
    -- hooks.do_channel(verb, me, fmt, mouren, msg_speak)
    --   answer 频道：先「出题」再作答；答对发历练并推送属性快照，
    --   让客户端面板不停留在旧值。其余频道一律返回 nil，交还内置分支。
    function hooks.do_channel(verb, me, fmt, mouren, msg_speak)
        if verb ~= "answer" then
            return nil
        end

        local idx = me:query_temp("exam/idx")

        if fmt == "出题" then
            idx = (idx % #QUESTIONS) + 1
            me:set_temp("exam/idx", idx)
            return { "考官：天下大考第 " .. idx .. " 题：" .. QUESTIONS[idx].q }
        end

        if idx == 0 then
            return { "考官：先说「出题」，听到题目再作答。" }
        end

        if fmt == QUESTIONS[idx].a then
            me:delete_temp("exam/idx")
            me:add("combat_exp", REWARD)
            -- 奖励入账即推送 RTNINFO：属性面板（总经验/战斗力）立即刷新
            me:FlushMyInfo()
            return { mouren .. " 答对了！奖历练 " .. REWARD .. " 点。" }
        end

        return { mouren .. " 答错了，再想想。" }
    end
end

return M
