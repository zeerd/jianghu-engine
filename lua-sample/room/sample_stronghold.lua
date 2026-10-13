-- sample_stronghold.lua  黑风寨大厅（副本房间）
-- 示例副本房间：展示副本房间的简单实现

return {
    type = "fbroom",
    name = "黑风寨大厅",

    create = function(self)
        self:set_name("黑风寨大厅")
        self:set("no_renwu", 1)
        self:set(
            "long",
            "这是黑风寨的大厅，四周挂满了刀枪剑戟。几个山贼正在大厅里巡逻。"
        )

        -- 添加副本管理员
        local npc = self:add_npc("sample_fb_manager")
        if npc then
            npc:set_name("黑风寨小头目", "xiao tou mu")
            npc:set("title", "副本管理员")
        end
    end,

    init = function(self, me)
        if not userp(me) then
            return
        end

        tell_object(me, "$HIR你进入了黑风寨大厅！")

        -- 刷出战斗NPC
        if self:query("spawned") == 0 then
            -- 刷出3个小喽啰
            for i = 1, 3 do
                self:add_npc("sample_bandit_low")
            end
            -- 刷出1个大当家
            self:add_npc("sample_bandit_boss")
            self:set("spawned", 1)
        end
    end,

    valid_leave = function(self, me, dir)
        -- 检查是否还有敌人
        local list = self:Get_ItemList()
        if list then
            for item in iter_items(list) do
                if item:is_character() and userp(item) == 0 then
                    if item:query("attitude") == "aggressive" then
                        tell_object(me, "$HIR还有敌人在场，你无法离开！")
                        return 0
                    end
                end
            end
        end
        return 1
    end,
}
