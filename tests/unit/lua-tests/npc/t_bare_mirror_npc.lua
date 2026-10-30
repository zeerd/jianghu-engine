-- t_bare_mirror_npc.lua  测试专用：什么都不覆写的镜像NPC 夹具
--
-- 验证镜像基模块的装配默认值：遁走自毁、低血自救、无凶手自毁的
-- 化灰台词——脚本自己一个钩子都不写，全靠 install 装配。
local npc = {}

local mirror_base = require("npc.mirror_base")

function npc.create(self)
    self:set_name("光杆镜像", "t bare mirror")
    self:set("max_hp", 100)
    self:set("hp", 100)
    self:set("max_mp", 50)
    self:set("mp", 50)
end

-- 装配放在全部脚本定义之后：自己定义过的不被覆盖
mirror_base.install(npc)

return npc
