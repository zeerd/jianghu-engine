-- quest.lua — 老李头的委托：用引擎的两个通用挂点写一个最小任务。
--
-- 引擎不认任何“任务”类型——任务是纯数据集逻辑，只挂在两个通用钩子上：
--   hooks.pre_talk(npc, me, ask)            每次对话咨询一次
--   hooks.pre_accept_object(npc, me, item)  每次把东西交给 NPC 咨询一次
-- 返回 { handled = true } = 本次由脚本接管（引擎不再走自己的询问菜单 /
-- 默认拒收）；返回 nil = 不适用，引擎照常走自身流程。
--
-- 对象参数是 userdata（可调用全部绑定方法），字符串为 GBK 编码。
-- 任务状态记在角色 dbase 的 quest/lilaotou 上，随角色存档一起走。

local M = {}

local NPC_NAME = "老李头"
local ITEM_NAME = "金创药"
local STATE_KEY = "quest/lilaotou"

-- 0=未接 1=已接 2=已交货 3=已领赏
local function state_of(me)
    return me:query(STATE_KEY)
end

function M.install(hooks)
    -- 对话：接委托 / 复命领赏。其余话题一律返回 nil，
    -- 交还老李头自己的询问菜单（山/山寨/商人/师父/排行榜）。
    function hooks.pre_talk(npc, me, ask)
        if npc:querystr("name") ~= NPC_NAME then
            return nil
        end

        local state = state_of(me)

        if ask == "委托" then
            if state == 0 then
                me:set(STATE_KEY, 1)
                npc:say("老朽腿脚不便，劳你替我到杂货老板那儿买一粒金创药来。", me)
            elseif state >= 2 then
                npc:say("药老朽已经收下了，多谢多谢。", me)
            else
                npc:say("金创药还没到手吗？杂货老板那儿就有卖。", me)
            end
            return { handled = true }
        end

        if ask == "复命" and state >= 2 then
            me:set(STATE_KEY, 3)
            me:add("combat_exp", 3000)
            npc:say("真是个守信的孩子。这点历练你收下，算是老朽的一点心意。", me)
            return { handled = true }
        end

        return nil
    end

    -- 交物：没接委托就交、或交的不是那味药，一律返回 nil——
    -- 引擎按默认逻辑拒收（“对方不要你的东西。”）
    function hooks.pre_accept_object(npc, me, item)
        if npc:querystr("name") ~= NPC_NAME then
            return nil
        end
        if state_of(me) ~= 1 then
            return nil
        end
        if item:querystr("name") ~= ITEM_NAME then
            return nil
        end

        me:set(STATE_KEY, 2)
        npc:say("老李头接过金创药，小心翼翼地收进怀里。", me)
        return { handled = true }
    end
end

return M
