-- t_broken_create_npc.lua  测试夹具：create 抛错的 NPC
-- create 先把对象置成可用状态再抛错，使加载器照常产出 NPC 实例，
-- 仅报错分支被触发（CLuaNpcT<T>::create 的 pcall 失败路径）。
local npc = {}

function npc.create(self)
    self:set_name("坏创建伙计", "broken create npc")
    self:set("hp", 1)
    error("create boom")
end

return npc
