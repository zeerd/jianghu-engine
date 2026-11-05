-- e2e_reload_noop_probe.lua  热重载 E2E noop 探针（fixture，房间类型）
-- 以房间脚本身份在开机 load_room_index 时被加载进 m_scripts——
-- check_for_reload 只扫已加载条目，探针必须真的在册，才测得到
--「mtime 变、内容不变 → 不产生重载」。
-- 剧本把它原样重写（filecopy 同一份 a/ 变体）；收尾由 reload_testbed.ps1
-- 断言日志中从无 "Lua auto-reload: e2e_reload_noop_probe"。
-- 内容必须始终与 a/ 下这份一致（改内容会破坏 noop 语义）。
return {
    type = "room",
    name = "热重载探针房",
}
