-- chanbanword.lua — 频道禁词表（可选的数据集配置）。
--
-- 开机时 CChatChannel::load_ban_words_from_lua 经
-- get_ban_words("chanbanword", "banned_words") 读取本文件；数据集不提供
-- 本文件时按空词表处理（禁词过滤不生效，不视为错误）。
--
-- 命中任一词即拒绝本次发言：
--   「对不起，因为您的语言里含有不文明字眼，系统拒绝发送。」
-- 匹配按 GBK 字节逐字进行（服务端字符串即 GBK），noise（市井）频道豁免。

return {
    banned_words = { "混蛋", "王八蛋" },
}
