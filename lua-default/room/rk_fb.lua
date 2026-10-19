-- rk_fb.lua  游戏大厅注册副本总控

-- 游客登录与月卡到期玩家都经 enter_register_hall 进一个独立的 rk_fb 实例：
-- 每人一局，互不挤同一房间；实例空置后由副本心跳自动回收，人被送回全局大厅。
-- 预载的大厅房就是 rk.lua 本身（base_name 仍是 rk），因此注册专员、注册白名单
-- 与大厅的年龄判定对实例一并照旧生效。
return {
    type = "fb",
    name = "rk_fb",

    create = function(self)
        self:set_name("游戏大厅")

        -- 预载大厅房并成为 start_map：move_in 的落点，
        -- 也是实例回收时把人送出去的出口
        self:load_fb_room("rk")
    end,
}
