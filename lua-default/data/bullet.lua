-- bullet.lua  公告数据
-- 供 C++ DownBulletInfo 从 Lua 加载。在用户登录时呈现在客户端。
-- 结构：返回表包含 bullet_lines 字段，为字符串数组，每行一条公告内容
-- 注意：文案中的颜色代码（$HIB, $HIM, $HIC 等）由客户端解析渲染

return {
    bullet_lines = {
        "　",
        "　　　　　　　　　　　　　　　　　　　",
        "　　　　　　　　　　　　$HIB☆　　　　　　　　　　　　　　　　$HIB☆",
        "　　$HIB☆　　　　　　　　　　　　　　　　　　　　　$HIB☆",
        "　　　　　$HIB☆　　　　　$HIM金　　剑　　江　　湖",
        "　　　　　　　　　　　　　　　　　　　　　　　　　　　　　　　$HIB☆",
        "　　$HIB☆　　　　$HICＢａｃｋ　ｔｏ　ｔｈｅ　Ｓｏｃｉｅｔｙ",
        "　　　　　　　　　　　　　　　　　　　　　　　　　　　　",
        "　　　　　　　　　　$HIB☆　　　　　　　　　　　　　　　　",
        "　　　　　　　　　　　　　　　　　　　　　　　　　　　　　$HIB☆",
        "　　　　　　　　　　　　",
        "　　　　　$HIB☆",
    },
}
