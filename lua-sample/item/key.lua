-- key.lua  铜钥匙
-- 示例物品：展示「开锁的钥匙」——CBox::handle_action 的钥匙支按
-- base_name=="key" 认钥匙（Box.cpp 的硬判，故本文件必须名为 key.lua）、
-- 按 password 相等认配对，所以钥匙的 password 必须与所开那把锁一致：
-- 本钥匙开的是山脚 sample_mountain_foot.lua 的锁着的箱子
-- （password 同为 123456）。教材世界里每把锁配一把钥匙即此意——
-- 改锁的暗号记得改这里，反之亦然。
-- 除 C++ 的 open 钥匙支外，本文件另带 do_use：持钥匙对锁具可上锁/开锁
-- （password 相认），演示「物品经 use 对容器施加状态」的走法。

return {
    create = function(self, nFlag)
        self:set_name("铜钥匙", "yao shi")
        self:set_weight(50)
        self:set("unit", "把")
        self:set("long", "一把黄铜钥匙，柄上缠着红线——像是开某把锁的原配。")
        self:set("value", 200)
        self:set("password", 123456)
    end,

    -- 声明可使用：客户端据此显示「使用」选项，use 命令也据此放行
    Can_Use = function(self)
        return 1
    end,

    -- 用钥匙上锁/开锁。与 CBox::handle_action 的钥匙支同规：按 password
    -- 相认配对——锁不上号的对象（非锁具、别人的锁）一律拒绝，不会对它
    -- 宣称开锁成功。锁着就打开，开着就锁上，两支各给私发回执。
    -- status 是字符串字段：query() 恒返回数值（对字符串取 0），须用 querystr。
    do_use = function(self, me, target)
        if not target or target:query("password") ~= self:query("password") then
            return notify_fail("这把钥匙对不上这把锁。")
        end

        if target:querystr("status") == "locked" then
            target:UnLocked()
            tell_object(me, "你用钥匙把锁打开了。")
            return 1
        end

        target:Locked("用钥匙锁上的", 0)
        tell_object(me, "你用钥匙把锁锁上了。")
        return 1
    end,
}
