-- monster_yongren.lua  宅院仆役（房屋自带）
-- 示例NPC：展示「房屋仆役」的写法。
--
-- 引擎创建房屋时（lua-default/room/house_base.lua 的 make_yongren）按固定
-- 基名 monster_yongren 载入本脚本，把它放进房里看家护院：
--   · 非主人进屋时，仆役会拦下「抢书」「打架」「借物不还」等行为
--     （见 house_base.lua 的 handle_action 与 valid_leave）；
--   · 房屋被卖掉或回收时，仆役随房屋一起消失。
--
-- 文件名必须与引擎写死的基名一致（monster_yongren），不能另起名；
-- 名字（set_name）只带一个参数，id 保持基名，引擎按基名认人。
--
local n = {}

function n.create(self)
    self:set_name("仆人", "man")

    self:set("gender", "女性")

    self:set("age", 20)
    self:set("per", 18 + math.random(0, 14))
    self:set("combat_exp", 20000)
    self:set("str", 40)
    self:set("no_kill", 1)

    self:set("infinity", 1)
    self:set("is_user", 1)
    self:set("no_huan", 1)
end

-- 认主：本房登记的主人（房主与第二房主）；再用 get_my_house 查访客在本城
-- 登记的住宅，与本房比对 object_id（绑定层每次 push 都造新 userdata，不能比对象本身）
local function is_master(self, me)
    local where = self:environment()
    if not where then
        return 0
    end

    if where:querystr("owner") == me:querystr("id") or where:querystr("second_owner") == me:querystr("id") then
        return 1
    end

    local mine = get_my_house(me)
    if mine and mine:object_id() == where:object_id() then
        return 1
    end

    return 0
end

function n.do_greeting1(self, pid)
    local env = self:environment()
    if not env then
        return
    end

    local pay = env:query("need_pays")
    if pay ~= 0 and self:query("warning_pays", "T") ~= pay then
        local master = env:present("object_id", pid)
        if not master then
            return
        end

        self:say(
            master:name() .. "主人，今日收到房屋管理帐单，合计" .. pay .. "。请过目。",
            master
        )
        self:set("warning_pays", pay, "T")
        self:add_menu_item("缴纳房屋帐单", "$0paymoney", master)
        self:send_menu(master)
    end

    if #self:querystr("welcome_master") > 0 then
        self:say(self:querystr("welcome_master"))
    end
end

function n.do_greeting2(self)
    self:say(self:querystr("welcome"))
end

function n.init(self, me)
    if not self:is_fighting() and me:is_user() then
        local list = me:get_item_list()
        if list then
            for obj in iter_items(list) do
                if obj:is_user() then
                    obj:move(load("room", START_ROOM))
                    if is_master(self, me) ~= 0 then
                        message(
                            "vision",
                            "$HIR主人，"
                                .. me:name()
                                .. "的身上藏了一个人叫"
                                .. obj:name()
                                .. "，可能意图不轨，赶快报官呀！",
                            self
                        )
                    else
                        message(
                            "vision",
                            "$HIR主人，你的身上藏了一个人叫"
                                .. obj:name()
                                .. "，可能意图不轨，赶快报官呀！",
                            self
                        )
                    end
                    message(
                        "vision",
                        "$HIR只见金光一闪，"
                            .. me:name()
                            .. "身上携带的"
                            .. obj:name()
                            .. "消失在一团迷雾中。",
                        self
                    )
                elseif obj:querystr("id") == "fight_room" then
                    message(
                        "vision",
                        "$HIR主人，快来看呀，"
                            .. me:name()
                            .. "的身上居然还有两个人在打架，好希奇哟！不过如此古怪，可能"
                            .. me:name()
                            .. "想意图不轨，还是赶快把他们扫地出门比较安全！",
                        self
                    )
                    me:move(load("room", global_room_name(math.random(math.max(1, room_count())) - 1)))
                end
            end
        end

        local hb = 0 - level2exp(me:query("level"))
        if me:query("balance") < hb and is_master(self, me) ~= 0 then
            tell_object(me, "你的家已经被查封，现在无法进入。")
            message(
                "room",
                me:environment(),
                "只听“乒”地一声，"
                    .. me:name()
                    .. "被人从房里一脚踢了出来，狼狈万状的逃开了。",
                me
            )
            me:move(load("room", global_room_name(math.random(math.max(1, room_count())) - 1)))
            me:add("hp", -100)
        else
            if is_master(self, me) ~= 0 then
                self:remove_call_out("do_greeting1")
                self:call_out("do_greeting1", 2, me:object_id())
            elseif #self:querystr("welcome") > 0 then
                self:remove_call_out("do_greeting2")
                self:call_out("do_greeting2", 2)
            end
        end
    end
end

-- 对话入口：认主后发房管菜单（帐单/改名/改描述/欢迎词/租售/提款），
-- 客人只有购物与租借菜单
function n.do_talk(self, me, ask)
    if is_master(self, me) ~= 0 then
        self:say("主人回来了。", me)

        local where = self:environment()
        if where and where:query("need_pays") ~= 0 then
            self:add_menu_item("缴纳房屋帐单", "$0paymoney", me)
        end

        self:add_menu_item("改名", "$2name $1 $2", me)
        self:add_menu_item("改描述", "$2desc $1 $2", me)
        self:add_menu_item("设置欢迎客人词", "$2welcome1 $1 $2", me)
        self:add_menu_item("设置欢迎主人词", "$2welcome2 $1 $2", me)
        self:add_menu_item("出售物品", "$0rent $1 2", me)
        self:add_menu_item("出租物品", "$0rent $1 1", me)
        self:add_menu_item("提款", "$0redraw $1", me)
    end

    self:add_menu_item("购买物品", "$0getobj $1 2", me)
    self:add_menu_item("租借物品", "$0getobj $1 1", me)
    self:send_menu(me)

    return 1
end

local function do_getobj(self, me, arg)
    local rent = tonumber(arg) or 0
    local list = self:get_item_list()

    if list then
        for obj in iter_items(list) do
            if obj:query("yongren/rent", "T") == rent then
                self:add_menu_item(
                    obj:name() .. " : " .. obj:query("yongren/price", "T"),
                    "$0giveme $1 " .. obj:object_id(),
                    me
                )
            end
        end
    end

    self:send_menu(me)
    return 1
end

local function do_giveme(self, me, arg)
    local obj = self:present("object_id", tonumber(arg) or 0)
    if not obj then
        return notify_fail("没有这样物品。")
    end

    if is_master(self, me) ~= 0 then
        -- move 绑定不返回结果（l_move 恒返回 nil），取回不校验返回值
        obj:move(me)
        obj:del("yongren", "T")
        self:say("你改变主意，不卖" .. obj:name() .. "了。", me)
        self:send_menu(me)
        return 1
    end

    local price = obj:query("yongren/price", "T")
    if price < 0 then
        return notify_fail("这样东西不卖！")
    end

    if pay_money("take", me, price) == 0 then
        return notify_fail("你没有这么多钱。")
    end

    if obj:query("yongren/rent", "T") == 1 then
        self:say("你租下了" .. obj:name() .. "，花掉" .. price .. "。", me)
    else
        obj:del("yongren", "T")
        self:say("你买下了" .. obj:name() .. ", 花掉" .. price .. "。", me)
    end

    -- move 绑定不返回结果（l_move 恒返回 nil），改查落点：进不了身就掉在房里
    obj:move(me)
    local landed = obj:environment()
    if not landed or landed:object_id() ~= me:object_id() then
        obj:move(me:environment())
    end

    self:send_menu(me)

    local where = self:environment()
    if where:lock_data(1) == 0 then
        return 0
    end
    where:load_from_local()

    where:add("yongren/balance", price * 4 // 5)
    where:save_to_local()
    where:lock_data(0)

    return 1
end

-- 售卖/出租合法性：钱币、商品与带主、禁止转手的物品一律不许经手
local function check_validsell(self, obj)
    local base_name = obj:querystr("base_name")

    if base_name == "coin" then
        return 0
    end
    if base_name == "trade" then
        return 0
    end
    if obj:query("index") ~= 0 then
        return 0
    end

    if obj:dbase("map", "no_give") then
        return 0
    end
    if obj:dbase("map", "no_drop") then
        return 0
    end
    if obj:dbase("map", "owner") then
        return 0
    end

    return 1
end

local function do_rent(self, me, arg)
    self:say("请选择租借或售卖的物品。", me)

    me:set("yongren/rent", tonumber(arg) or 0, "T")

    local list = me:get_item_list()
    if list then
        for obj in iter_items(list) do
            if not obj:is_character() and check_validsell(self, obj) ~= 0 then
                self:add_menu_item(obj:name(), "$0value $1 " .. obj:object_id(), me)
            end
        end
    end

    self:send_menu(me)
    return 1
end

local function do_value(self, me, arg)
    local obj = me:present("object_id", tonumber(arg) or 0)
    if not obj then
        return notify_fail("你身上没有这样物品！")
    end

    me:set("yongren/obj", tonumber(arg) or 0, "T")

    if me:query("yongren/rent", "T") == 1 then
        self:say(obj:name() .. "价值" .. obj:query("value") .. "，请定租借价格！", me)
    else
        self:say(obj:name() .. "价值" .. obj:query("value") .. "，请定出售价格！", me)
    end

    self:add_menu_item("定价", "$1price $1 $2", me)
    self:add_menu_item("放弃", "", me)
    self:send_menu(me)

    return 1
end

local function do_price(self, me, arg)
    local price = tonumber(arg) or 0
    local obj = me:present("object_id", me:query("yongren/obj", "T"))
    if not obj then
        return notify_fail("请选定物品！")
    end

    obj:move(self)
    obj:set("yongren/rent", me:query("yongren/rent", "T"), "T")
    obj:set("yongren/price", price, "T")

    self:say("OK，设定物品价格" .. price .. "。", me)
    self:send_menu(me)

    me:del("yongren", "T")
    return 1
end

local function do_withdraw(self, me)
    local where = self:environment()
    if where:lock_data(1) == 0 then
        return 0
    end
    where:load_from_local()

    local balance = where:query("yongren/balance")
    where:del("yongren/balance")

    where:save_to_local()
    where:lock_data(0)

    if balance == 0 then
        return notify_fail("您并没有售出物品。")
    end

    pay_money("give", me, balance)
    self:say("您获得了" .. balance .. "的利润。", me)
    self:send_menu(me)

    log_info(me:name(1) .. "(" .. me:id(1) .. ")从" .. self:name(1) .. "身上提取" .. balance .. "的利润。")

    return 1
end

-- 入参为服务器内部 GBK 编码（脚本加载期已统一转 GBK，NPC/房间/物品三条
-- 指令分发口径一致：字面量与入参同编码），直接落库
local function do_name(self, me, arg)
    local name = arg
    local msg = check_legal_name(name)
    if #msg > 0 then
        self:say(msg, me)
        self:send_menu(me)
        return 1
    end

    self:set_name(name)
    self:update_me()

    local where = self:environment()
    if where:lock_data(1) == 0 then
        return 0
    end
    where:load_from_local()

    where:set("yongren/name", name)
    where:save_to_local()
    where:lock_data(0)

    tell_object(me, "OK.")
    return 1
end

-- 入参为服务器内部 GBK 编码（见 do_name 的说明），直接落库
local function do_desc(self, me, arg)
    local desc = arg
    local len = #desc
    if len < 1 or len > 500 then
        self:say("给您的仆人描述写得简洁漂亮点。", me)
        self:send_menu(me)
        return 1
    end

    if valid_string(desc) == 0 then
        return 0
    end

    self:set("long", desc)

    local where = self:environment()
    if where:lock_data(1) == 0 then
        return 0
    end
    where:load_from_local()

    where:set("yongren/long", desc)
    where:save_to_local()
    where:lock_data(0)

    return 1
end

-- 入参为服务器内部 GBK 编码（见 do_name 的说明），直接落库
local function do_welcome1(self, me, arg)
    local welcome = arg
    local len = #welcome
    if len < 1 or len > 250 then
        self:say("给您仆人的欢迎词写得简洁漂亮点。", me)
        self:send_menu(me)
        return 1
    end

    if valid_string(welcome) == 0 then
        return 0
    end

    self:set("welcome", welcome)

    local where = self:environment()
    if where:lock_data(1) == 0 then
        return 0
    end
    where:load_from_local()

    where:set("yongren/welcome", welcome)
    where:save_to_local()
    where:lock_data(0)

    return 1
end

-- 入参为服务器内部 GBK 编码（见 do_name 的说明），直接落库
local function do_welcome2(self, me, arg)
    local welcome = arg
    local len = #welcome
    if len < 1 or len > 250 then
        self:say("给您仆人的欢迎词写得简洁漂亮点。", me)
        self:send_menu(me)
        return 1
    end

    if valid_string(welcome) == 0 then
        return 0
    end

    self:set("welcome_master", welcome)

    local where = self:environment()
    if where:lock_data(1) == 0 then
        return 0
    end
    where:load_from_local()

    where:set("yongren/welcome_master", welcome)
    where:save_to_local()
    where:lock_data(0)

    return 1
end

function n.handle_action(self, comm, me, arg)
    -- 打斗与咆哮一律不理会（抢书的 roar 由房间层 house_base 另行拦下）
    if comm == "kill" or comm == "fight" or comm == "cast" then
        message("vision", "$n向$N皱了皱眉.", me, self)
        return 1
    end

    if is_master(self, me) ~= 0 then
        if comm == "name" then
            return do_name(self, me, arg)
        elseif comm == "desc" then
            return do_desc(self, me, arg)
        elseif comm == "welcome1" then
            return do_welcome1(self, me, arg)
        elseif comm == "welcome2" then
            return do_welcome2(self, me, arg)
        elseif comm == "rent" then
            return do_rent(self, me, arg)
        elseif comm == "value" then
            return do_value(self, me, arg)
        elseif comm == "price" then
            return do_price(self, me, arg)
        elseif comm == "redraw" then
            return do_withdraw(self, me)
        end
    end

    if comm == "getobj" then
        return do_getobj(self, me, arg)
    elseif comm == "giveme" then
        return do_giveme(self, me, arg)
    end

    -- 绑定签名是 (self, comm, me, arg)，与 register_clerk.lua 同口径
    return self:base_handle_action(comm, me, arg)
end

function n.accept_object(self, me, obj)
    if obj:query("yongren/rent", "T") ~= 0 then
        return 1
    end
    return 0
end

return n
