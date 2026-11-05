-- channel_config.lua 扩展频道配置（f1-f4、answer 及数据集新增频道）
--
-- 本文件属数据集内容：引擎默认（lua-default）不带扩展频道，各数据集自带
-- 本文件（教材 lua-sample、生产数据集 jianghu-dataset 各有一份，互不覆盖）。
--
-- 契约（读取方：CChatChannel::create → get_channel_config，启动期读一次）：
--   · 键 = 频道动词（发言命令字），值 = 该频道字段表；条目整体生效，
--     缺字段即该字段未设置（不做逐字段回退）。
--   · C++ 只硬编码 10 个机制频道（sys/new/news/wiz/chat/ad/noise/rumor/
--     party/corps），扩展频道全在本文件：文件缺失 = 未配置，这些频道
--     不注册（服务器照常启动）；修改本文件需重启生效（启动期只读）。
--   · channel 字段恒由 C++ 按键名写入，本文件写同值即可。
--   · 数值字段（引擎按整数读取，写 1/0；数字或 true/false 皆可）：
--     wiz_only / wiz_use / all_receive / newbie / use_mp / interMain_emote
--     ——新增数值字段需同步扩展 C++ 白名单（ChatChannel.cpp
--     is_numeric_channel_field），否则按字符串存入、读取恒为 0。
--   · 其余字段为字符串（msg_speak / msg_emote / msg_color / anonymous），
--     文本写 "true" 不会被当布尔解析。
--   · $颜色码与 %s 占位符语义与基础频道一致。

return {
    f1 = {
        msg_speak = "$HIM【%s】$COM%s",
        msg_emote = "$HIM【%s】%s",
        msg_color = "$COM",
        channel = "f1",
        newbie = 1,
    },
    f2 = {
        msg_speak = "$HIG【%s】$COM%s",
        msg_emote = "$HIG【%s】%s",
        msg_color = "$COM",
        channel = "f2",
        newbie = 1,
    },
    f3 = {
        msg_speak = "$HIW【%s】$COM%s",
        msg_emote = "$HIW【%s】%s",
        msg_color = "$COM",
        channel = "f3",
        newbie = 1,
    },
    f4 = {
        msg_speak = "$HIW【%s】$COM%s",
        msg_emote = "$HIW【%s】%s",
        msg_color = "$COM",
        channel = "f4",
        newbie = 1,
    },
    answer = {
        msg_speak = "$YEL【考试】%s: %s",
        msg_color = "$YEL",
        channel = "answer",
        newbie = 1,
    },
}
