-- resident_base.lua 居民区通用逻辑
-- 住宅区的购房登记、管理费结算、住户名录与卖房迁出。
-- 地图脚本只需声明 type = "resident" 并在 create 里 set("resident", 属区名)，
-- 引擎在脚本加载期自动调用 install 装配本模块，脚本无需 require 本模块；
-- 脚本自定义的同名钩子优先（install 只补齐缺失项）。

local resident_base = {}

local DEFAULT_HOUSE_MIN_AGE = 16

-- 缺省单点容量；属区在 create 里 set("capacity", N) 即可声明本点上限
local DEFAULT_CAPACITY = 70

-- 阶梯地价（金）：住户每满 5 户升一档，超过最高档按封顶价
local HOUSE_PRICES = {
    50,
    60,
    72,
    85,
    100,
    125,
    150,
    180,
    216,
    260,
    312,
    375,
    450,
    540,
    650,
}

-- 住户登记表的键列表（无登记时返回空表）
local function house_keys(self)
    local entries = self:querymap_keys("house")
    if not entries then
        return {}
    end

    local keys = {}
    for _, item in ipairs(entries) do
        keys[#keys + 1] = item.key
    end
    return keys
end

-- 载入本地房屋：force 为真时不校验存档是否存在
local function get_local_house(self, menpai, force)
    local room_name = "house_" .. self:querystr("resident") .. "_" .. menpai
    local room = load_room(room_name)
    if not room then
        return nil
    end
    if not force and room:Load() == 0 then
        return nil
    end
    return room
end

-- 计算拖欠天数
function resident_base.count_days(self, owner)
    local key = "house/" .. owner
    if not self:querymap(key) then
        return 0
    end
    local last = self:query(key .. "/time")
    return math.floor((current_time() - last) / 86400)
end

-- 房产价格
function resident_base.house_price(self)
    local fixed = self:query("policy/house_price")
    if fixed > 0 then
        return fixed
    end

    local keys = house_keys(self)
    if #keys == 0 then
        return 500000
    end

    local level = math.floor(#keys / 5)
    if level > 14 then
        return 6500000 * 2
    end
    return HOUSE_PRICES[level + 1] * 10000
end

-- 是否可以申请住房：本点声明的 capacity 生效，未声明按缺省上限
function resident_base.can_apply_house(self)
    local capacity = self:query("capacity")
    if capacity <= 0 then
        capacity = DEFAULT_CAPACITY
    end
    return #house_keys(self) < capacity and 1 or 0
end

-- 购房年龄门槛
function resident_base.house_min_age(self)
    return DEFAULT_HOUSE_MIN_AGE
end

-- 写入/注销住户登记。登记表按户主 id 分片，字段为 name/short/owner/time
function resident_base.set_house(self, menpai, m)
    if self:LockData(1) == 0 then
        return 0
    end
    self:Load()

    if m then
        for k, v in pairs(m) do
            self:set("house/" .. menpai .. "/" .. k, v)
        end
    else
        self:del("house/" .. menpai)
    end

    self:Save()
    self:LockData(0)
    return 1
end

-- 更新注册：回写住户名录，并补齐房屋的场景描述与短名
function resident_base.update_register(self, house)
    local m = {
        name = house:querystr("name"),
        owner = house:querystr("second_owner"),
        time = house:query("pay_time"),
    }

    local owner_key = house:querystr("owner")
    local owner = find_online(owner_key)
    local flag1 = false
    if not owner then
        flag1 = true
        owner = add_user()
        owner:set("id", owner_key)
        owner:LoadFromServer()
    end

    local second_key = house:querystr("second_owner")
    if second_key ~= "" then
        local owner2 = find_online(second_key)
        local flag2 = false
        if not owner2 then
            flag2 = true
            owner2 = add_user()
            owner2:set("id", second_key)
            owner2:LoadFromServer()
        end

        house:set("long", string.format("这里是%s和%s的共享房间。", owner:name(1), owner2:name(1)))
        m.short = string.format("%s和%s", owner:name(1), owner2:name(1))
        if flag2 then
            destruct(owner2)
        end
    else
        house:set("long", string.format("这里是%s的私人房间。", owner:name()))
        m.short = owner:name(1)
    end

    if flag1 then
        destruct(owner)
    end

    resident_base.set_house(self, owner_key, m)
end

-- 删掉一处住房：注销住户、迁回起始城市并销毁房体
function resident_base.remove_house(self, house)
    local owner_key = house:querystr("owner")
    local owner = find_online(owner_key)
    local flag1 = false

    if not owner then
        flag1 = true
        owner = add_user()
        owner:set("id", owner_key)
        owner:LoadFromServer()
    end

    owner:del("resident")
    owner:set("start_city", START_ROOM)
    owner:Save()

    if flag1 then
        destruct(owner)
    end

    resident_base.set_house(self, owner_key, nil)

    house:remove_all_box()
    house:remove()
    destruct(house)
end

-- 自动清欠：拖欠满 30 天的住户从银行扣费，扣不清则收回房产
function resident_base.clear_house(self)
    for _, key in ipairs(house_keys(self)) do
        local days = resident_base.count_days(self, key)
        if days >= 30 then
            local room = get_local_house(self, key)
            if not room then
                -- 房屋已不存在，注销登记
                resident_base.set_house(self, key, nil)
                return
            end

            local price = resident_base.house_price(self)
            if room:query("size") < 2 then
                price = math.floor(price / 2)
            end
            price = math.floor(days * price / 250)

            local owner = find_online(key)
            local flag1 = false
            if not owner then
                flag1 = true
                owner = add_user()
                owner:set("id", room:querystr("owner"))
                owner:LoadFromServer()
            end

            if owner:query("balance") > -2000000000 then
                owner:add_balance(-price)
            end

            if owner:query("balance") >= price then
                room:del("need_pays")
                room:set("pay_time", current_time())
                resident_base.update_register(self, room)
                owner:Save()
                if flag1 then
                    destruct(owner)
                end
                return
            end

            if flag1 then
                destruct(owner)
            end

            local msg = string.format(
                "%s由于房主%s(%s)长期拖欠物业管理费，被%s政府抵押收回。",
                self:querystr("house/" .. key .. "/name"),
                self:querystr("house/" .. key .. "/short"),
                key,
                self:querystr("resident")
            )
            do_channel(nil, nil, "news", msg)
            -- 一次只收回一座房屋：注销后立即返回，避免物件链表错乱
            return
        end
    end
end

-- 第二房主的"回家"入口
function resident_base.check_second_owner(self, me)
    local owner = me:id(1)
    for _, key in ipairs(house_keys(self)) do
        if self:querystr("house/" .. key .. "/owner") == owner and key ~= owner then
            self:AddMenuItem(
                string.format("进入%s", self:querystr("house/" .. key .. "/name")),
                string.format("$0enter 0 %s", key),
                me
            )
        end
    end
end

-- 属区统计与菜单：自定义 do_look 的房间播完自己的场景后调用本函数拼装
-- 属区信息与菜单（SendMenu 在此发出）。do_look = 场景长描述 + 本函数。
function resident_base.zone_look(self, me)
    local has_house = self:querymap("house")
    local msg
    if has_house then
        msg = string.format(
            "这里是%s城的住宅区。目前这里有%d家住户。",
            self:querystr("resident"),
            #house_keys(self)
        )
    else
        msg = string.format("这里是%s城的住宅区。目前这里没有任何住户。", self:querystr("resident"))
    end
    self:say(msg, me)

    if me:querystr("resident") == self:querystr("resident") then
        self:AddMenuItem("回家", string.format("$0enter 0 %s", me:id(1)), me)
    elseif resident_base.can_apply_house(self) == 1 then
        self:AddMenuItem(string.format("申请成为%s居民", self:querystr("resident")), "$0applyhouse", me)
    end

    resident_base.check_second_owner(self, me)

    local invite = me:querystr_temp("invite")
    if invite ~= "" then
        local u = find_online(invite)
        if u then
            self:AddMenuItem(string.format("去%s的家", u:name()), string.format("$0enter 0 %s", u:id(1)), me)
        end
    end

    if has_house then
        self:AddMenuItem("查看有哪些住户", "$0checkhouse", me)
    end

    if me:querystr("resident") == self:querystr("resident") then
        local room = get_my_house(me)
        if room then
            self:AddMenuItem(string.format("卖掉%s", room:name()), "$0sellhouse 0 0", me)
        end
    end

    self:SendMenu(me)
    return 1
end

-- 查看住宅区：先播房间脚本声明的长描述，再走属区统计与菜单
function resident_base.do_look(self, me)
    local long = self:querystr("long")
    if long ~= "" then
        self:say(long, me)
    end

    return resident_base.zone_look(self, me)
end

-- 申请成为居民
function resident_base.applyhouse(self, me)
    if me:querystr("resident") ~= "" then
        self:say(string.format("你已经申请成为%s人了。", me:querystr("resident")), me)
        self:SendMenu(me)
        return 1
    end

    if me:query("age") < resident_base.house_min_age(self) then
        self:say("你还没成年，做不了房主。", me)
        self:SendMenu(me)
        return 1
    end

    self:AddMenuItem("申请单人间", "$0applyroom 0 1", me)
    self:AddMenuItem("申请双人套房", "$0applyroom 0 2", me)
    self:SendMenu(me)
    return 1
end

-- 申请单人间时房款折半
function resident_base.need_money(self, me)
    local money = resident_base.house_price(self)
    if me:query_temp("house/size") == 1 then
        money = math.floor(money / 2)
    end
    return money
end

-- 选房型：确认现金足够后进入房间属性设置
function resident_base.applyroom(self, me, arg)
    local coin = me:present("coin")

    me:set_temp("house/size", 2)
    if arg == "1" then
        me:set_temp("house/size", 1)
    end

    local money = resident_base.need_money(self, me)
    if not coin or coin:query("value") < money then
        self:say(string.format("申请房间需要%d金，请备好现金再来。", money), me)
        self:SendMenu(me)
        return 1
    end

    resident_base.menulist(self, me)
    return 1
end

-- 房间属性设置菜单
function resident_base.menulist(self, me)
    self:AddMenuItem("设置房间名称", "$2housename 0 $2", me)
    self:AddMenuItem("设置房间描述", "$2housedesc 0 $2", me)

    if me:query_temp("house/size") > 1 then
        self:AddMenuItem("设置第二房主", "$2addmaster 0 $2", me)
    end

    if me:querystr_temp("house/name") ~= "" then
        self:AddMenuItem("申请房间", "$0registerhouse", me)
    end

    self:SendMenu(me)
end

-- 设置第二房主（须在线）
function resident_base.addmaster(self, me, arg)
    local owner = find_online(arg)
    if not owner then
        self:say("对不起，请这个人在线时再来注册房主。", me)
    else
        me:set_temp("house/owner", arg)
    end

    resident_base.menulist(self, me)
    return 1
end

-- 设置房间名称（入参为服务器内部 GBK，与脚本字面量同编码）
function resident_base.housename(self, me, arg)
    local name = arg
    local msg = check_legal_name(name)
    if msg ~= "" then
        self:say(msg, me)
    else
        me:set_temp("house/name", name)
    end

    resident_base.menulist(self, me)
    return 1
end

-- 设置房间描述（入参为服务器内部 GBK，与脚本字面量同编码）
function resident_base.housedesc(self, me, arg)
    local desc = arg
    local len = #desc
    if len < 1 or len > 500 then
        self:say("给您的房间描述写得简洁漂亮点。", me)
    else
        me:set_temp("house/desc", desc)
    end

    resident_base.menulist(self, me)
    return 1
end

-- 注册房间：收取房款、建房、登记住户并迁移起始城市
function resident_base.registerhouse(self, me)
    local house_name = me:querystr_temp("house/name")
    if house_name == "" then
        self:AddMenuItem("请给您的房间起个名字", "$1housename 0 $2", me)
        self:SendMenu(me)
        return 1
    end

    if resident_base.can_apply_house(self) == 0 then
        self:say("对不起，本居民区已无空房，不能办理申请手续！", me)
        self:SendMenu(me)
        return 1
    end

    local money = resident_base.need_money(self, me)
    if player_pay(me, money) == 0 then
        self:say("对不起，你的现金不足！", me)
        self:SendMenu(me)
        return 1
    end

    local room = get_local_house(self, me:querystr("id"), true)
    if not room then
        self:say("此房间不存在!", me)
        self:SendMenu(me)
        return 1
    end

    room:set_name(me:querystr_temp("house/name"))
    room:set("desc", me:querystr_temp("house/desc"))
    room:set("size", me:query_temp("house/size"))
    room:set("owner", me:id(1))
    room:set("belong", self:querystr("base_name"))
    room:set("second_owner", me:querystr_temp("house/owner"))
    room:set("pay_time", current_time())
    room:del("fail_ob")

    room:remove_all_doors()
    room:setup()

    me:set("resident", self:querystr("resident"))
    me:delete_temp("house")

    if me:querystr("title") == "普通百姓" then
        me:set("title", string.format("%s百姓", self:querystr("resident")))
        me:UpdateMe()
    end

    me:set("start_city", room:querystr("base_name"))
    me:Save()

    -- 登记在前、存房在后：房屋 long 由登记补写，先存会丢描述
    resident_base.update_register(self, room)
    room:Save()

    self:AddMenuItem(string.format("进入%s。", room:name()), string.format("$0enter 0 %s", me:id(1)), me)
    self:SendMenu(me)

    log_info(
        "%s(%s)在%s申请住房%s(%s)，花费%d。",
        me:name(1),
        me:id(1),
        self:name(1),
        room:name(1),
        room:querystr("base_name"),
        money
    )

    return 1
end

-- 列出住户名单
function resident_base.checkhouse(self, me)
    if not self:querymap("house") then
        return 0
    end

    for _, key in ipairs(house_keys(self)) do
        self:AddMenuItem(
            string.format(
                "%s(%s)",
                self:querystr("house/" .. key .. "/name"),
                self:querystr("house/" .. key .. "/short")
            ),
            string.format("$0enter 0 %s", key),
            me
        )
    end

    self:SendMenu(me)
    return 1
end

-- 回家/闯入
function resident_base.backhome(self, me, arg)
    if not self:querymap("house/" .. arg) then
        self:say("你要进入的房间不在本区！", me)
        self:SendMenu(me)
        return 1
    end

    local is_owner = arg == me:querystr("id")
    local is_second = self:querystr("house/" .. arg .. "/owner") == me:querystr("id")
    local is_invited = arg == me:querystr_temp("invite")

    if is_owner or is_second or is_invited then
        local room = get_local_house(self, arg)
        if not room then
            self:say("此房间不存在!", me)
            self:SendMenu(me)
            return 1
        end

        if not is_owner and not is_second then
            if not room:present(arg) then
                self:say("主人不在家，不能闯入！", me)
                self:SendMenu(me)
                return 1
            end
        end

        MessageVision(
            me,
            "chat",
            string.format("%s进入了%s。", me:name(), self:querystr("house/" .. arg .. "/name")),
            me
        )
        me:move(room)
        MessageVision(me, "chat", string.format("%s走了进来。", me:name()), me)
        return 1
    end

    self:say(string.format("你试图闯入%s，结果没有成功！", self:querystr("house/" .. arg .. "/name")), me)
    self:SendMenu(me)
    return 1
end

-- 卖掉自己的住房
function resident_base.sellhouse(self, me, arg)
    local my_id = me:querystr("id")
    if not self:querymap("house/" .. my_id) then
        self:say("你要卖掉的房间不在本区！", me)
        self:SendMenu(me)
        return 1
    end

    local room = get_my_house(me)
    if not room or room:Load() == 0 then
        if room then
            destruct(room)
        end
        return notify_fail("你的房子已经卖掉了．")
    end

    if room:querystr("owner") ~= my_id then
        return notify_fail("这不是你的房子。")
    end

    local price = resident_base.house_price(self)
    if room:query("size") == 1 then
        price = math.floor(price / 2)
    end
    price = math.floor(price / 2)

    local shui = math.floor(price * resident_base.count_days(self, my_id) / 500)
    price = price - shui
    if price < 0 then
        price = 1
    end

    if arg == "0" then
        self:say(
            string.format(
                "%s目前价值%d。卖掉房子意味着你从本市迁出。真的要卖吗？",
                room:name(),
                price
            ),
            me
        )
        self:AddMenuItem("决定卖了", "$0sellhouse 0 1", me)
        self:AddMenuItem("离开", "", me)
        self:SendMenu(me)
        return 1
    end

    self:say(string.format("你卖掉了%s，获得%d。", room:name(), price), me)
    self:SendMenu(me)

    pay_player(me, price)

    log_info(
        "%s(%s)在%s卖掉房子%s(%s)，获利%d。From (%s)",
        me:name(1),
        me:id(1),
        self:name(1),
        room:name(1),
        room:querystr("base_name"),
        price,
        GetIP(me)
    )

    resident_base.remove_house(self, room)
    return 1
end

-- 维护窗口到期后清欠
function resident_base.reset(self)
    self:base_reset()

    local wait = self:query_temp("wait_clear")
    if wait ~= 0 then
        self:set_temp("wait_clear", wait - 1)
        return
    end

    self:Load()
    self:set_temp("wait_clear", 40 + random(40))
    resident_base.clear_house(self)
end

-- 载入存档并初始化房间
function resident_base.setup(self)
    self:set_temp("wait_clear", 40 + random(40))
    self:Load()
    self:base_setup()
end

-- 房屋命令分发；其余命令交回房间基类
function resident_base.handle_action(self, comm, me, arg, tar)
    if comm == "enter" then
        return resident_base.backhome(self, me, arg)
    elseif comm == "applyhouse" then
        return resident_base.applyhouse(self, me)
    elseif comm == "applyroom" then
        return resident_base.applyroom(self, me, arg)
    elseif comm == "housename" then
        return resident_base.housename(self, me, arg)
    elseif comm == "housedesc" then
        return resident_base.housedesc(self, me, arg)
    elseif comm == "registerhouse" then
        return resident_base.registerhouse(self, me)
    elseif comm == "addmaster" then
        return resident_base.addmaster(self, me, arg)
    elseif comm == "checkhouse" then
        return resident_base.checkhouse(self, me)
    elseif comm == "sellhouse" then
        return resident_base.sellhouse(self, me, arg)
    end

    return self:base_handle_action4(comm, me, arg, tar)
end

-- 把本模块的行为装入住宅区脚本表（脚本自定义的同名钩子优先）。
-- 由引擎在 type = "resident" 的脚本加载期自动调用（见
-- CLuaEngine::apply_room_class_mixins），地图脚本无需手动调用；
-- 仅脚本内显式装配（如测试夹具）才直接使用本函数。
function resident_base.install(room)
    for name, value in pairs(resident_base) do
        if name ~= "install" and type(value) == "function" then
            if room[name] == nil then
                room[name] = value
            end
        end
    end

    if room.can_save == nil then
        room.can_save = true
    end
    if room.resident_area == nil then
        room.resident_area = true
    end

    return room
end

return resident_base
