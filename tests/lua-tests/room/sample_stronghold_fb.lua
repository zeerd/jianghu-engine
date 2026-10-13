-- sample_stronghold_fb.lua  黑风寨副本总控
-- 示例副本：展示副本总控的简单实现

return {
    type = "fb",
    name = "黑风寨",

    create = function(self)
        self:set_name("黑风寨")
        self:set("long", "这是黑风寨副本，山贼的大本营。")
        self:set("no_renwu", 1)

        -- 加载副本房间
        self:load_fb_room("黑风寨大厅")
    end,

    desc = function(self, me)
        tell_object(me, "$HIR你闯入了黑风寨！小心山贼的伏击！")
        tell_object(me, "$HIY山寨大厅里有几个山贼在巡逻，还有一个武功高强的大当家。")
    end,
}
