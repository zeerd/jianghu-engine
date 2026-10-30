-- t_mirror_npc.lua  测试专用：镜像NPC 夹具（显式装配基模块）
--
-- 由 test_npc_mirror.cpp 驱动：本脚本自己 require 镜像基模块并
-- install 进脚本表（die / chat_msg / chat_msg_combat 等缺省钩子
-- 随装配补齐），本脚本只留 reward_killer 这个可覆写钩子的标记实现——
-- 它经 tell_object 给传入的角色发一行，C++ 侧据收到的消息断言分派
-- 确实到达脚本。
local npc = {}

local mirror_base = require("npc.mirror_base")

function npc.create(self)
    self:set_name("镜像夹具", "t mirror")
    self:set("max_hp", 300)
    self:set("hp", 300)
    self:set("max_mp", 100)
    self:set("mp", 100)
end

function npc.reward_killer(self, me)
    tell_object(me, "镜像的奖励发出去了。")
end

-- 装配放在全部脚本定义之后：自己定义过的不被覆盖
mirror_base.install(npc)

return npc
