-- box_draw_log.lua  开盲盒记档
-- 示例数据模块：演示引擎 DB 链四连——
--   db_register_ddl：require 期登记建表语句（建表由开机 prepare_tables 末尾
--     统一执行，登记必须落在 Lua 初始化期；同一条 DDL 重复登记由引擎去重）；
--   get_db_type：按库方言给 DDL——sqlite 用 INTEGER PRIMARY KEY，
--     mysql 用 AUTO_INCREMENT + 与连接一致的 gb2312；
--   db_execute：写入一条开盒记录；
--   db_query：读回该角色的累计开盒次数。
-- 独立表 sample_box_draw 只记谁(who)/何时(ts)/开出什么(prize)，不碰引擎生产表。

local M = {}

local TABLE = "sample_box_draw"

local function init_table()
    local ddl
    if get_db_type() == "sqlite" then
        ddl = [[
            CREATE TABLE IF NOT EXISTS `sample_box_draw` (
                `idx` INTEGER PRIMARY KEY,
                `who` TEXT NOT NULL,
                `ts` INTEGER NOT NULL,
                `prize` TEXT NOT NULL
            )
        ]]
    else
        ddl = [[
            CREATE TABLE IF NOT EXISTS `sample_box_draw` (
                `idx` INT NOT NULL AUTO_INCREMENT,
                `who` TINYTEXT NOT NULL,
                `ts` INT NOT NULL,
                `prize` TINYTEXT NOT NULL,
                PRIMARY KEY (`idx`)
            ) ENGINE = MyISAM CHARSET = gb2312
        ]]
    end
    db_register_ddl(ddl)
end

-- 本模块由 data/hooks.lua 在开机期 require：建表语句要在 prepare_tables
-- 之前登记完毕，拆盒时的 require 只会命中缓存（登记也由引擎去重）。
init_table()

-- 记一次开盒：写 who/ts/prize，读回该谁的累计次数；失败返回 nil + 错误串
function M.record(who, prize)
    local name = string.gsub(who, "'", "''")
    local ok, err = db_execute(
        string.format("INSERT INTO `%s` (`who`, `ts`, `prize`) VALUES ('%s', %d, '%s')", TABLE, name, os.time(), prize)
    )
    if not ok then
        return nil, err
    end

    local rows, qerr = db_query(string.format("SELECT COUNT(*) FROM `%s` WHERE `who` = '%s'", TABLE, name))
    if not rows or not rows[1] then
        return nil, qerr
    end
    return tonumber(rows[1][1])
end

return M
