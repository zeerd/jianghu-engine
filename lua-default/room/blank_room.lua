-- blank_room.lua  空白房间素体
-- 剧情需要"当场捏一个临时房间"时的底子：没有出口、没有 NPC、没有摆设，
-- 只提供一个能被 move 进去的容器，供藏匿角色、暂存待销毁尸体这类用途。
-- 调用方按裸脚本名加载：load("room", "blank_room")。
-- 刻意不声明 name：素体不进中文地名索引，既不占键位，也不会被
-- global_room_name 的随机传送抽中；需要唯一身份时另建房间脚本。
return {
    type = "room",

    create = function(self)
        self:set_name("空房间")
    end,

    handle_action = function(self, comm, me, arg, tar)
        if comm == "get" or comm == "drop" or comm == "give" or comm == "use" then
            return 1
        end
        return 0
    end,
}
