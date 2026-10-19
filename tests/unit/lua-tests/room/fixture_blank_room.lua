-- fixture_blank_room.lua  测试专用：无任何钩子的空脚本房间
--
-- 供 LuaRoom.inl 的 C++ 兜底支用例：create/init/setup/handle_action/
-- valid_leave/reload_script 在脚本缺钩子时回落 C++ 实现。
return {
    type = "room",
    name = "空钩子房间",
}