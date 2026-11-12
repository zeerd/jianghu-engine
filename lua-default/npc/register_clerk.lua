-- register_clerk.lua  角色注册专员（游戏大厅 rk）
--
-- 交互模式参考"凶猛的狗铸造兵刃"（ccjh quanzhou_xmdog）：
--   talk 菜单 → 菜单项把命令回灌进 handle_action → 分步收集
--   姓名/密码/性别/天赋 → register_char 落库，注册成功后凭账号 ID + 密码登录。
-- 注意：self 是 C++ userdata，脚本自定义函数必须提取为 local function 调用，
-- 不能用 self:xxx() 调用脚本函数。

local npc = {}

function npc.create(self)
    self:set_name("注册专员", "zhu ce")
    self:set("title", "游戏角色注册专员")
    self:set("gender", "男性")
    self:set("age", 40)
    self:set("long", "一位温和的中年人，专门为初入江湖者办理角色注册手续。")
    self:set("attitude", "friendly")
    self:set("chat_chance", 3)
end

function npc.init(self, me)
    if is_user(me) and not self:is_fighting() then
        self:remove_call_out("do_greeting")
        self:call_out("do_greeting", 1, me:object_id())
    end
end

function npc.do_greeting(self, pid)
    local env = self:environment()
    if not env then
        return
    end
    local who = env:present("object_id", pid)
    if not who then
        return
    end
    message("vision", "$n冲$N招了招手，说道：想入江湖，先注册一个角色吧！", who, self)
end

function npc.chat_msg(self)
    local r = random(3)
    if r == 0 then
        return "有谁想闯荡江湖的，来找我注册角色啊。"
    elseif r == 1 then
        return "注册很简单的，名字、密码、性别、天赋，一会儿就好。"
    else
        return "注册完成以后，记得用账号和密码重新登录啊。"
    end
end

-- ===================== 角色注册流程 =====================

-- 注册菜单：每一步答完都回到这个菜单（与凶猛的狗的铸造菜单同构）。
-- 菜单项文案即格式说明书：玩家在首次输入之前就该看得到规则，
-- 不能等答错一次才通过错误提示倒推。
local function reg_menu(self, me)
    self:add_menu_item("设置角色姓名", "$2regname $1 $2", me)
    self:add_menu_item("设置登录密码", "$2regpass $1 $2", me)
    self:add_menu_item("设置邮箱地址", "$2regemail $1 $2", me)
    self:add_menu_item("选择性别", "$0reggender $1", me)
    self:add_menu_item("设置天赋属性", "$2regstat $1 $2", me)
    self:add_menu_item("确认创建角色", "$0regdone $1", me)
    self:send_menu(me)
end

-- 开始/继续注册
local function do_regchar(self, me)
    me:set("regclerk/step", 1, "T")
    self:say("设置角色姓名。", me)
    self:say("设置登录密码（4~22 个字符）。", me)
    self:say("设置邮箱地址（须含 @，如 name@example.com）。", me)
    self:say("选择性别。", me)
    self:say("设置天赋属性（以空格分隔的四个数字）", me)
    self:say("（依次为力量、智慧、体质、敏捷）", me)
    self:say("（单项 10~30、总和 84，如 21 21 21 21）。", me)
    reg_menu(self, me)
    return 1
end

local function do_regname(self, me, arg)
    if #arg == 0 then
        tell_object(me, "你想取个什么名字？")
        reg_menu(self, me)
        return 1
    end

    local msg = check_legal_name(arg)
    if #msg > 0 then
        tell_object(me, msg)
        reg_menu(self, me)
        return 1
    end

    me:set("regclerk/name", arg, "T")
    tell_object(me, "Ok.")
    reg_menu(self, me)
    return 1
end

local function do_regpass(self, me, arg)
    if #arg < 4 or #arg > 22 then
        tell_object(me, "密码长度必须在 4 到 22 个字符之间。")
    else
        me:set("regclerk/pass", arg, "T")
        tell_object(me, "Ok.")
    end
    reg_menu(self, me)
    return 1
end

local function do_regemail(self, me, arg)
    if #arg == 0 then
        tell_object(me, "请输入邮箱地址。")
        reg_menu(self, me)
        return 1
    end

    -- 简单格式校验：必须包含 @ 且 @ 前后非空
    local at = string.find(arg, "@")
    if not at or at == 1 or at == #arg then
        tell_object(me, "邮箱格式不正确：须包含 @，且 @ 前后不能为空，例如 name@example.com。")
        reg_menu(self, me)
        return 1
    end

    me:set("regclerk/email", arg, "T")
    tell_object(me, "Ok.")
    reg_menu(self, me)
    return 1
end

local function do_reggender(self, me, arg)
    if arg == "男性" or arg == "女性" then
        me:set("regclerk/gender", arg, "T")
        tell_object(me, "Ok.")
        reg_menu(self, me)
        return 1
    end

    self:say("你想注册为男性还是女性？", me)
    self:add_menu_item("男性", "$0reggender $1 男性", me)
    self:add_menu_item("女性", "$0reggender $1 女性", me)
    self:send_menu(me)
    return 1
end

-- 天赋属性：由玩家自己指定。依次输入力量、智慧、体质、敏捷四个数字，
-- 单项 10~30、总和必须正好 84（与巫师账号初始四维 21/21/21/21 同口径）。
local function do_regstat(self, me, arg)
    local nums = {}
    for num in string.gmatch(arg, "%d+") do
        nums[#nums + 1] = tonumber(num)
    end

    if #nums ~= 4 then
        tell_object(
            me,
            "请输入四个数字（依次为力量、智慧、体质、敏捷），用空格分开，例如："
                .. "21 21 21 21；单项 10~30，四项总和必须正好 84。"
        )
        reg_menu(self, me)
        return 1
    end

    local sum = 0
    for i = 1, 4 do
        if nums[i] < 10 or nums[i] > 30 then
            tell_object(me, "每一项属性都必须在 10 到 30 之间。")
            reg_menu(self, me)
            return 1
        end
        sum = sum + nums[i]
    end

    if sum ~= 84 then
        tell_object(me, "四项属性之和必须正好是 84，你现在给的是 " .. sum .. "。")
        reg_menu(self, me)
        return 1
    end

    me:set("regclerk/str", nums[1], "T")
    me:set("regclerk/int", nums[2], "T")
    me:set("regclerk/con", nums[3], "T")
    me:set("regclerk/dex", nums[4], "T")
    tell_object(me, "Ok.")
    reg_menu(self, me)
    return 1
end

local function do_regdone(self, me)
    local name = me:dbase_temp("str", "regclerk/name")
    local pass = me:dbase_temp("str", "regclerk/pass")
    local email = me:dbase_temp("str", "regclerk/email")
    local gender = me:dbase_temp("str", "regclerk/gender")

    if #name == 0 then
        self:say("你还没有设置角色姓名呢。", me)
        reg_menu(self, me)
        return 1
    end
    if #pass == 0 then
        self:say("你还没有设置登录密码呢。", me)
        reg_menu(self, me)
        return 1
    end
    if #email == 0 then
        self:say("你还没有设置邮箱地址呢。", me)
        reg_menu(self, me)
        return 1
    end
    if #gender == 0 then
        self:say("你还没有选择性别呢。", me)
        reg_menu(self, me)
        return 1
    end
    if me:query("regclerk/str", "T") == 0 then
        self:say("你还没有设置天赋属性呢。", me)
        reg_menu(self, me)
        return 1
    end

    local id, err = register_char(
        name,
        pass,
        email,
        gender,
        me:query("regclerk/str", "T"),
        me:query("regclerk/int", "T"),
        me:query("regclerk/con", "T"),
        me:query("regclerk/dex", "T")
    )
    if id == 0 then
        self:say((err ~= nil and #err > 0) and err or "注册失败，请稍后再试。", me)
        reg_menu(self, me)
        return 1
    end

    me:set("regclerk/done", 1)
    me:del("regclerk/step", "T")
    me:del("regclerk/name", "T")
    me:del("regclerk/pass", "T")
    me:del("regclerk/email", "T")
    me:del("regclerk/gender", "T")
    me:del("regclerk/str", "T")
    me:del("regclerk/int", "T")
    me:del("regclerk/con", "T")
    me:del("regclerk/dex", "T")

    self:say("注册成功！你的角色账号是 " .. id .. "，请牢记。", me)
    self:say(
        "退出游戏后重新连接，用这个账号和刚才设置的密码登录，就能进入江湖了。",
        me
    )
    return 1
end

-- talk 入口：列出注册相关话题（与凶猛的狗的 do_talk 菜单同构）
function npc.do_talk(self, me, ask)
    me:set("regclerk/talked", 1, "T")
    self:add_menu_item("与注册专员谈关于创建角色的事", "$0regchar $1", me)
    self:send_menu(me)
    return 1
end

-- 菜单回灌的命令统一从这里分发（命令名是 ASCII，不受 GBK/UTF-8 差异影响）
function npc.handle_action(self, comm, me, arg)
    if comm == "regchar" then
        return do_regchar(self, me)
    end

    if me:query("regclerk/step", "T") ~= 0 then
        if comm == "regname" then
            tell_object(me, "设置角色姓名。")
            return do_regname(self, me, arg)
        elseif comm == "regpass" then
            tell_object(me, "设置登录密码。")
            return do_regpass(self, me, arg)
        elseif comm == "regemail" then
            tell_object(me, "设置邮箱地址。")
            return do_regemail(self, me, arg)
        elseif comm == "reggender" then
            tell_object(me, "设置性别。")
            return do_reggender(self, me, arg)
        elseif comm == "regstat" then
            tell_object(me, "设置属性点。")
            return do_regstat(self, me, arg)
        elseif comm == "regdone" then
            tell_object(me, "完成注册。")
            return do_regdone(self, me)
        end
    end

    return self:base_handle_action(comm, me, arg)
end

return npc
