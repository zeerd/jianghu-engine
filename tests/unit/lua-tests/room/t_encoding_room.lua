-- t_encoding_room.lua — 指令分发编码契约夹具
--
-- 脚本源码以 UTF-8 存盘、加载期统一转 GBK（load_file_as_gbk），因此本文件
-- 里的中文字面量在运行时是 GBK 字节；房间分发（CLuaRoomT::handle_action /
-- valid_leave）必须投递原始 GBK 入参，字面量才能直接比较。
-- 若分发侧单方面转码（历史行为），下面的比较恒不相等、返回 0。
return {
    type = "room",
    name = "t_encoding_room",

    handle_action = function(self, comm, me, arg, tar)
        if arg == "测试" then
            return 11
        end
        if comm == "改名" then
            return 12
        end
        return 0
    end,

    valid_leave = function(self, me, dir)
        if dir == "出口" then
            return 13
        end
        return 1
    end,
}
