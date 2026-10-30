-- key.lua  铜钥匙
-- 示例物品：展示「开锁的钥匙」——CBox::handle_action 的钥匙支按
-- base_name=="key" 认钥匙（Box.cpp 的硬判，故本文件必须名为 key.lua）、
-- 按 password 相等认配对，所以钥匙的 password 必须与所开那把锁一致：
-- 本钥匙开的是山脚 sample_mountain_foot.lua 的锁着的箱子
-- （password 同为 123456）。教材世界里每把锁配一把钥匙即此意——
-- 改锁的暗号记得改这里，反之亦然。

return {
    create = function(self, nFlag)
        self:set_name("铜钥匙", "yao shi")
        self:set_weight(50)
        self:set("unit", "把")
        self:set("long", "一把黄铜钥匙，柄上缠着红线——像是开某把锁的原配。")
        self:set("value", 200)
        self:set("password", 123456)
    end,
}
