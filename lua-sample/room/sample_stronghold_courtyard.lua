-- sample_stronghold_courtyard.lua  黑风寨庭院（副本房间）
-- 示例副本房间：展示副本门机制的入口房间
-- 大厅不在副本总控里预加载：庭院通往大厅的出口是副本门（CFBDoor），
-- 首次通过时经父副本 load_fb_room 按需创建，两房同属一个副本实例。

return {
    type = "fbroom",
    name = "黑风寨庭院",

    create = function(self)
        self:set_name("黑风寨庭院")
        self:set("no_renwu", 1)
        self:set(
            "long",
            "这是黑风寨的庭院，黄土铺地，两侧的木桩上插着几把生锈的钢刀。北面聚义大厅里传来山贼的喝骂声。"
        )

        -- 副本门：通向大厅。第三个参数是大厅一侧的出口名（来路出口），
        -- 进门时经 init_door 告知客户端。
        self:add_door("聚义大厅", "黑风寨大厅", "庭院大门")
    end,

    init = function(self, me)
        if not is_user(me) then
            return
        end

        tell_object(me, "$HIR你潜入了黑风寨的庭院！")

        -- 刷出守门喽啰（仅首次进入时刷一次）
        if self:query("spawned") == 0 then
            for i = 1, 2 do
                self:add_npc("sample_bandit_low")
            end
            self:set("spawned", 1)
        end
    end,
}
