-- sample_stronghold_fb.lua  黑风寨副本总控
-- 示例副本：展示副本总控的简单实现

return {
    type = "fb",
    name = "黑风寨",

    create = function(self)
        self:set_name("黑风寨")
        self:set("long", "这是黑风寨副本，山贼的大本营。")
        self:set("no_renwu", 1)

        -- 只预加载入口房间（成为 start_map）。大厅不在此预加载：
        -- 由庭院通往大厅的副本门首次通过时经 load_fb_room 按需创建。
        self:load_fb_room("黑风寨庭院")
    end,

    desc = function(self, me)
        tell_object(me, "$HIR你闯入了黑风寨！小心山贼的伏击！")
        tell_object(me, "$HIY庭院里有守门的喽啰，北面聚义大厅里是武功高强的大当家。")
    end,
}
