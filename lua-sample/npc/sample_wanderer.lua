-- sample_wanderer.lua  云游道人
-- 示例NPC：展示 random_move 随机走动 + 自我续排 call_out 的巡逻。
--
-- 走向由出口图说了算：副本房（黑风寨）与主图之间没有门——副本只能经副本
-- 总控 move_in 进入——随机取出口走不进去；房屋同理（住宅区那一侧只出不进，
-- 进门走「回家」）。所以这条巡游只在主图打转，不会闯进副本。

local npc = {}

function npc.create(self)
    self:set_name("云游道人", "yun you dao ren")
    self:set("gender", "男性")
    self:set("age", 45)
    self:set("long", "一个背着葫芦的道人，脚步不停地在山道上游历。")
    self:set("attitude", "friendly")

    -- 闲聊概率闸：与脚本侧 chat_msg 台词成对使用，掷中即回一句
    self:set("chat_chance", 5)

    self:call_out("do_wander", 20)
end

-- 云游道人的山道闲话：返回台词即房间广播（chat_chance 掷中时调用）
function npc.chat_msg(self)
    local r = math.random(3)
    if r == 1 then
        return "云游四方，山高水长。"
    elseif r == 2 then
        return "这青云山的景致，一年比一年好了。"
    end
    return "葫芦里的酒快见底了，下山得打一壶。"
end

-- 巡游一步：随机取一个出口走过去，末尾再排自己——周期效果只有「延迟函数
-- 末尾再次注册自己」这一种写法（docs/call_out.md）。心跳停摆的条件是
-- 「不在战斗、没有挂起的延迟函数」，自我续排的 do_wander 让它一直挂着。
function npc.do_wander(self)
    self:random_move()
    self:call_out("do_wander", 20)
end

return npc
