-- house_base.lua 玩家私人房间通用逻辑
-- 以 house_ 为前缀的动态实例房共享本脚本：置产与住户登记由属区脚本
--（room/resident_base.lua）完成，本脚本负责房屋自身的居住行为——柜子
-- 与仆役、门牌、房管命令、租借品看管与管理费结算。
-- 状态字段统一落在 dbase：owner / second_owner / belong / size / desc
-- / need_pays / pay_time / yongren。

local house = {}

-- 租借品规则（起租物标记、查租、外人指令截获）独立成模块
local house_rent = require("room.house_rent")

-- 属区登记逻辑（地价/欠租天数/住户名录）由住宅区基类模块承载：
-- 直调模块函数，self 传属区房间对象，不经 C++ 契约接口
local resident_base = require("room.resident_base")

-- 命中 base_name 前缀的房间由本脚本实例化；房屋可存盘
house.prefix = "house_"
house.can_save = true

-- 查租与租品判定委托给租借品规则模块，脚本表入口保持不变
house.check_rent = house_rent.check_rent

-- 加一个柜子：可装 50 件，且不许装人
local function make_box(self, box_name)
    local box = self:Add_Box()
    box:set_name(box_name, "box")
    box:set("max_items", 50)
    box:set("no_drop_man", 1)
end

-- 补一名仆役：可由 yongren 子映射定制外貌、欢迎词与功力
local function make_yongren(self)
    local npc = load_npc("monster_yongren")
    if not npc then
        return
    end

    if self:querymap("yongren") then
        local fields = {
            { "name", true },
            { "welcome", false },
            { "long", false },
            { "welcome_master", false },
        }
        for _, field in ipairs(fields) do
            local value = self:querystr("yongren/" .. field[1])
            if field[2] then
                npc:set_name(value)
            else
                npc:set(field[1], value)
            end
        end
        npc:set("combat_exp", self:query("yongren/combat_exp"))
    end

    if npc:query_exp() == 0 then
        npc:set("combat_exp", 20000)
    end

    setup_char(npc)
    npc:move(self)

    self:set_temp("yongren", npc:object_id())
end

-- 房屋已不存在：延迟析构，等主循环统一冲刷
function house.remove_me(self)
    destruct(self)
end

-- 检查是否携带租借品：主人与第二主人豁免，其余一律搜身
-- 回写住户登记：同步房名与住客名录，并刷新房屋的场景描述
function house.update_register(self)
    local zone = load_room(self:querystr("belong"))
    if zone:query(IS_RESIDENT) == 0 then
        do_channel(
            nil,
            nil,
            "sys",
            string.format("update:房间%s所注册的区域%s不是可利用的居住区！", self:name(), zone:name())
        )
        return
    end

    resident_base.update_register(zone, self)
    self:Save()
end

-- 检查拖欠的管理费：够扣就从银行划走，否则记成待结帐单
function house.check_pays(self)
    local belong = self:querystr("belong")
    if belong == "" then
        return
    end

    local zone = load_room(belong)
    if zone:query(IS_RESIDENT) == 0 then
        do_channel(
            nil,
            nil,
            "sys",
            string.format(
                "check_pays:房间%s所注册的区域%s不是可利用的居住区！",
                self:name(),
                zone:name()
            )
        )
        return
    end

    local days = resident_base.count_days(zone, self:querystr("owner"))
    local price = resident_base.house_price(zone)
    if self:query("size") < 2 then
        price = math.floor(price / 2)
    end

    price = math.floor(days * price / 250)
    if price <= 0 then
        self:set("need_pays", 0)
        return
    end

    local owner_key = self:querystr("owner")
    local owner = find_online(owner_key)
    local temporary = false
    if not owner then
        temporary = true
        owner = add_user()
        owner:set("id", owner_key)
        owner:LoadFromServer()
    end

    if owner:query("balance") >= price then
        owner:add_balance(-price)
        self:del("need_pays")
        self:set("pay_time", current_time())
        house.update_register(self)
        owner:Save()
        if temporary then
            destruct(owner)
        end
        return
    end

    if temporary then
        destruct(owner)
    end
    self:set("need_pays", price)
end

-- 建房：默认房名、睡眠房标记、柜子与仆役
function house.create(self)
    self:set_name("房间", "house")

    self:set("sleep_room", 1)
    self:set("area", "house")

    self:set("saveobjects", 1)
    self:set("max_items", 20)

    make_box(self, "柜子")
    local count = self:query("box_count")
    for i = 1, count - 1 do
        make_box(self, string.format("柜子%d", i + 1))
    end

    make_yongren(self)
end

-- 按房型出对应地图，装上回属区的门；存档完好的房顺带查一次管理费
function house.setup(self)
    local door = self:add_door("出口", self:querystr("belong"), self:querystr("name"), "out")
    door:set_name(self:querystr("belong"))

    if self:query("size") < 2 then
        self:set("map", "玩家民居1")
    else
        self:set("map", "玩家民居2")
    end

    if self:query("fail_ob") == 0 then
        house.check_pays(self)
    end
end

-- 周期维护：存档消失则拆房；仆役走失即补；顺带查欠租
function house.reset(self)
    if combatd_query("no_check_house") == 0 then
        if self:Load() == 0 then
            self:call_out("remove_me", 1)
            return
        end
    end

    if not find_npc(self:query_temp("yongren")) then
        make_yongren(self)
    end

    house.check_pays(self)
end

-- 门牌：房间长描述、房主留言与房管菜单
function house.do_look(self, me)
    self:say(self:Long(), me)
    self:say(self:querystr("desc"), me)

    local owner = self:querystr("owner")
    local second_owner = self:querystr("second_owner")
    local me_id = me:id(1)
    if owner == me_id or second_owner == me_id then
        self:AddMenuItem("拒绝客人来访", "$2kickout 0 $2", me)
        self:AddMenuItem("修改房间名称", "$2housename 0 $2", me)
        self:AddMenuItem("修改房间描述", "$2housedesc 0 $2", me)

        if owner == me_id and self:query("size") > 1 then
            self:AddMenuItem("邀请其他人同住", "$2addmaster 0 $2", me)
        end
    end

    self:SendMenu(me)
    return 1
end

-- 租借的物品不允许带出房间
function house.valid_leave(self, me, dir)
    if dir == "出口" then
        local who = self:present_name("monster_yongren", IS_NPC)
        local obj = house.check_rent(self, me)
        if who and obj then
            return notify_fail(string.format("%s伸手拦住你，说：把%s留下！！", who:name(), obj:name()))
        end
    end

    return self:base_valid_leave(me, dir)
end

-- 设置房间名称（入参为服务器内部 GBK，与脚本字面量同编码，直接落库）
function house.housename(self, me, arg)
    local name = arg
    local msg = check_legal_name(name)
    if msg ~= "" then
        self:say(msg, me)
        self:SendMenu(me)
        return 1
    end

    if self:LockData(1) == 0 then
        return 0
    end

    if self:Load() == 0 then
        self:call_out("remove_me", 1)
        return 1
    end

    self:set_name(name)

    house.update_register(self)
    self:LockData(0)

    tell_object(me, "OK.")
    return 1
end

-- 设置房间描述（入参为服务器内部 GBK，与脚本字面量同编码，直接落库）
function house.housedesc(self, me, arg)
    local desc = arg
    local len = #desc
    if len < 1 or len > 500 then
        self:say("给您的房间描述写得简洁漂亮点。", me)
        self:SendMenu(me)
        return 1
    end

    if valid_string(desc) == 0 then
        return 0
    end

    if self:LockData(1) == 0 then
        return 0
    end

    if self:Load() == 0 then
        self:call_out("remove_me", 1)
        return 1
    end

    self:set("desc", desc)

    self:Save()
    self:LockData(0)

    return 1
end

-- 设置第二房主（须在线）
function house.addmaster(self, me, arg)
    if self:querystr("owner") ~= me:id(1) then
        self:say("对不起，只有户主才邀请其他人同住。", me)
        self:SendMenu(me)
        return 1
    end

    if self:query("size") < 2 then
        self:say("对不起，这个房间不能再住其他人了。", me)
        self:SendMenu(me)
        return 1
    end

    if not find_online(arg) then
        self:say("对不起，请这个人在线时再来注册房主。", me)
        self:SendMenu(me)
        return 1
    end

    self:set("second_owner", arg)
    house.update_register(self)

    tell_object(me, "OK.")
    return 1
end

-- 赶出去：随身的租用品留下，人送回属区
function house.do_kick(self, me, arg)
    local who = find_online(arg)
    if not who then
        return notify_fail("你想踢谁？")
    end

    if self:querystr("owner") == who:id(1) then
        return notify_fail("没搞错，人家可是房主哎！！")
    end

    if house_rent.same_object(who:environment(), self) then
        message_vision("$HIY$N飞起一脚，将$n踢出了门外！", me, who)

        local rented = {}
        house_rent.each_item(who, function(obj)
            if house_rent.is_rented(obj) then
                rented[#rented + 1] = obj
            end
            return nil
        end)
        for _, obj in ipairs(rented) do
            obj:move(self)
            message_vision("$n从$N身上掉落下来。", who, obj)
        end

        who:move(load_room(self:querystr("belong")))
    else
        tell_object(me, string.format("你拒绝让%s登门造访！", who:name(1)))
    end

    if who:querystr_temp("invite") == self:querystr("owner") then
        who:delete_temp("invite")
    end

    return 1
end

-- 交纳房屋帐单：不带参数时摊帐单，带参数时结算
function house.do_paymoney(self, me, arg)
    local pay = self:query("need_pays")
    if pay == 0 then
        return 0
    end

    if #arg == 0 then
        self:say(
            string.format(
                "一张%s的管理费用结算单。\n\n截止今日累计费用合计%d金。\n\n"
                    .. "注意：当拖欠时间超过一个月，您将失去本房间的居住权。",
                self:name(),
                pay
            ),
            me,
            "房屋帐单"
        )
        self:AddMenuItem("结算", "$0paymoney 0 1", me)
        self:AddMenuItem("不予理会", "", me)
    else
        if self:LockData(1) == 0 then
            return 0
        end

        if self:Load() == 0 then
            self:call_out("remove_me", 1)
            return 0
        end

        house.check_pays(self)
        pay = self:query("need_pays")

        if player_pay(me, pay) == 0 then
            self:say("对不起，现金不足！", me)
        else
            self:del("need_pays")
            self:set("pay_time", current_time())
            house.update_register(self)
        end

        self:LockData(0)
        self:say(string.format("支付了%d金结算帐单！", pay), me)
    end

    self:SendMenu(me)
    return 1
end

-- 房管命令（仅主人与第二主人）。返回 1 表示已处理；nil 表示放行
local function dispatch_house_command(self, comm, me, arg)
    if comm == "housename" then
        return house.housename(self, me, arg)
    elseif comm == "housedesc" then
        return house.housedesc(self, me, arg)
    elseif comm == "addmaster" then
        return house.addmaster(self, me, arg)
    elseif comm == "kickout" then
        return house.do_kick(self, me, arg)
    elseif comm == "paymoney" then
        return house.do_paymoney(self, me, arg)
    end
    return nil
end

-- 房屋命令分发；拦截不住的命令交回房间基类
function house.handle_action(self, comm, me, arg, tar)
    local yongren = self:present_name("monster_yongren", IS_NPC)

    -- 任何时候都不可以使用 roar
    if yongren and comm == "cast" and arg == "force roar" then
        message_vision("$N拦住$n道：“想抢书？出去！”", yongren, me)
        return 1
    end

    if self:querystr("owner") == me:id(1) or self:querystr("second_owner") == me:id(1) then
        local handled = dispatch_house_command(self, comm, me, arg)
        if handled then
            return handled
        end
    elseif yongren then
        local blocked = house_rent.intercept_foreign(self, comm, me, arg, tar, yongren)
        if blocked then
            return blocked
        end
    end

    return self:base_handle_action4(comm, me, arg, tar)
end

return house
