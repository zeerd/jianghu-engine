-- login.lua 登录后校验（默认简版）
-- 仅检查四维属性之和是否合法

local login = {}

-- 名字校验
-- @param name  玩家名字（UTF-8）
-- @return "" 表示合法，非空字符串为错误信息
function login.check_name(name)
    if #name < 2 or #name > 12 then
        return "对不起，你的中文名字必须是 1 到 6 个中文字。"
    end
    return ""
end

-- 登录后校验入口
--
-- @param me        玩家对象（jianghu_object userdata）
-- @param newenter  number: 1=首次登入（新建或断线重连后重新加载存档），
--                  0=重连线（同帐号重复登录替换旧连接）
--                  首次登入时会执行等级重算、套装技能取消等额外逻辑。
-- @return number   0=校验通过；
--                  1=校验失败已自行处置（惩罚放行，登录流程照常）；
--                  2=拒绝准入：引擎把玩家送回游戏大厅并断开，
--                    说明文案由脚本自行 tell_object。
function login.on_login(me, newenter)
    local total = me:query("str") + me:query("con") + me:query("int") + me:query("dex")
    local limit = 84 + me:query("reward/attrib")
    if total > limit then
        me:set_temp("属性", 1)
        tell_object(me, "\n$HIY您的四项先天属性超过了合理范围。")
        me:move(load_room("jy"))
        return 1
    end
    return 0
end

return login
