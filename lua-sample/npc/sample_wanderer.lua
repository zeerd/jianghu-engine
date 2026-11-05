-- sample_wanderer.lua  云游道人
-- 示例NPC：展示 random_move 随机走动 + 自我续排 call_out 的巡逻。
--
-- 走向由出口图说了算：副本房（黑风寨）与主图之间没有门——副本只能经副本
-- 总控 MoveIn 进入——随机取出口走不进去；房屋同理（住宅区那一侧只出不进，
-- 进门走「回家」）。所以这条巡游只在主图打转，不会闯进副本。

local npc = {}

function npc.create(self)
    self:set_name("云游道人", "yun you dao ren")
    self:set("gender", "男性")
    self:set("age", 45)
    self:set("long", "一个背着葫芦的道人，脚步不停地在山道上游历。")
    self:set("attitude", "friendly")

    self:call_out("do_wander", 20)
end

-- 巡游一步：随机取一个出口走过去，末尾再排自己——周期效果只有「延迟函数
-- 末尾再次注册自己」这一种写法（docs/call_out.md）。心跳停摆的条件是
-- 「不在战斗、没有挂起的延迟函数」，自我续排的 do_wander 让它一直挂着。
function npc.do_wander(self)
    self:random_move()
    self:call_out("do_wander", 20)
end

return npc
