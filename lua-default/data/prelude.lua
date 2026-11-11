-- prelude.lua — 启动期前导脚本（内容层）
--
-- 装载：绑定注册完毕后由引擎执行（src/lua/LuaBind.cpp
-- lua_bind_run_prelude）；script_root 下的同名文件整体优先于本文件，
-- 内容层可用一个完整的 prelude 覆盖默认根的前导。
--
-- 入参：jianghu_object 元表（其 __index 指向自身）——
--   local index = ...     -- 方法表：index.foo = function(ob) ... end
--                          -- 对应脚本侧 ob:foo()
-- 在全局环境定义的函数即脚本侧全局 API。
--
-- 分工：C++ 绑定只暴露引擎原子能力；脚本习惯的调用形态在这里组合，
-- 不复制引擎内部逻辑（引擎机制以绑定入参为准）。
local index = ...

-- 日志族：引擎只暴露 log(level, fmt, ...)，这里按级别名组合脚本侧调用形态
-- （六级齐备：trace/debug/info/warn/error/fatal，级别由 log 的入参分派）
function log_trace(...)
    return log("trace", ...)
end
function log_debug(...)
    return log("debug", ...)
end
function log_info(...)
    return log("info", ...)
end
function log_warn(...)
    return log("warn", ...)
end
function log_error(...)
    return log("error", ...)
end
function log_fatal(...)
    return log("fatal", ...)
end

-- 等级换算族：数据源 data.level_exp（覆盖 1..176），公式与引擎内部的
-- CChar::level2skill / skill2level / level2_exp / exp2level 同式同源；
-- 引擎自身仍用它的缓存直接服务 C++ 调用，不经过这里。
local EXP_LV_MAX = 176 -- 表覆盖上限，与 MainConfig.h NORMAL_MAX_LEVEL 同值
local exp_table = nil

local function level_exp_data()
    if exp_table ~= nil then
        return exp_table or false
    end
    local ok, tbl = pcall(require, "data.level_exp")
    exp_table = (ok and type(tbl) == "table") and tbl or false
    return exp_table or false
end

-- 数据表缺席（require 失败）时的近似表：分段拟合，与引擎侧备选同源
local function approx_exp(lv)
    if lv <= 5 then
        return 50 * lv * lv - 50 * lv
    elseif lv <= 75 then
        return math.floor(8.16 * lv ^ 2.81)
    elseif lv <= 157 then
        return math.floor(1.99 * lv ^ 3.21)
    end
    local v = math.exp(0.304593 * lv - 31.61993)
    if v >= 2147483647 then
        v = 2147483647
    end
    return math.floor(v)
end

local function level2exp_raw(lv)
    if lv < 1 then
        return 0
    end
    if lv >= EXP_LV_MAX then
        return 1917735009 + lv * 100000000
    end
    local tbl = level_exp_data()
    local v = tbl and tbl[lv]
    return v or approx_exp(lv)
end

-- level2exp(lv) -> 人物等级 -> 升级所需累计经验
function level2exp(lv)
    return level2exp_raw(lv)
end

-- exp2level(exp) -> 经验 -> 人物等级
function exp2level(exp)
    local lv = 0
    while level2exp_raw(lv) <= exp do
        lv = lv + 1
    end
    return lv - 1
end

-- skill2level(sk) -> 技能点 -> 人物等级
function skill2level(sk)
    if sk > 84 then
        return (sk - 84) * 2 // 7 + 25
    end
    if sk > 24 then
        return (sk - 24) // 3 + 5
    end
    return sk // 4 - 1
end

-- level2skill(lv) -> 人物等级 -> 技能点上限
function level2skill(lv)
    if lv < 5 then
        return (lv + 1) * 4
    end
    if lv < 25 then
        return 24 + (lv - 5) * 3
    end
    return 84 + (lv - 25) * 7 // 2
end

-- 称谓族：引擎只暴露 respect(kind, ob)，这里按旧名组合脚本侧调用形态
function query_respect(ob)
    return respect("respect", ob)
end
function query_rude(ob)
    return respect("rude", ob)
end
function query_self(ob)
    return respect("self", ob)
end
function query_self_rude(ob)
    return respect("self_rude", ob)
end

-- 对象谓词族：引擎只暴露 is(ob, KIND)（恒返回布尔），这里按旧名组合
function is_user(ob)
    return is(ob, "user")
end
function is_wizard(ob)
    return is(ob, "wizard")
end
function is_living(ob)
    return is(ob, "living")
end

-- is_character 是对象方法（ob:is_character()），挂在方法表上
function index.is_character(ob)
    return is(ob, "character")
end

-- 时间信息族：日历时间用 os.date 组合（引擎只给心跳 tick current_time）；
-- wday 归一到 0=周日，与 C 侧 tm_wday 同口径（os.date 为 1=周日）
function current_time_info()
    local t = os.date("*t")
    t.wday = t.wday - 1
    return t
end

-- 随机取容器内一物：按类型属性过滤、跳过 exclude（比 object_id），
-- 与原引擎实现同一 RNG（random）、同 50 件上限；空容器/非对象回 nil
function random_get_item(ob, ntype, exclude)
    if type(ob) ~= "userdata" then
        return nil
    end
    local list = ob:get_item_list()
    if not list then
        return nil
    end
    ntype = ntype or IS_ITEM
    local excl_id = (type(exclude) == "userdata") and exclude:object_id() or nil
    local pool = {}
    for item in iter_items(list) do
        if item:query(ntype) ~= 0 and item:object_id() ~= excl_id then
            pool[#pool + 1] = item
            if #pool >= 50 then
                break
            end
        end
    end
    if #pool == 0 then
        return nil
    end
    return pool[random(#pool) + 1]
end

-- 查找 me 所在房间内的角色：present("object_id") 只在房间容器内找，
-- IS_CHAR 过滤非角色物件；me 非角色 / target 为 0 / 无环境一律 nil
function get_room_char(me, target)
    if not me or not me:is_character() or not target or target == 0 then
        return nil
    end
    local room = me:environment()
    if not room then
        return nil
    end
    local ob = room:present("object_id", target)
    if ob and ob:query(IS_CHAR) ~= 0 then
        return ob
    end
    return nil
end

-- 亲属/师徒/伴侣判定（技能查看权限）：师徒按同门 + master_id，
-- 伴侣按 couple/id 互认，再看环境互为对方；对象身份比 object_id
-- （每次压入的 userdata 不是同一个值，不能用 == 比对象）
function is_relative(me, who)
    if not me or not who or not me:is_character() or not who:is_character() then
        return false
    end
    local bangpai = me:querystr("family/family_name")
    if
        bangpai ~= ""
        and me:querystr("family/master_id") == who:querystr("id")
        and bangpai == who:querystr("family/family_name")
    then
        return true
    end
    if me:querystr("couple/id") == who:querystr("id") and who:querystr("couple/id") == me:querystr("id") then
        return true
    end
    local menv = me:environment()
    local whenv = who:environment()
    if menv and menv:object_id() == who:object_id() then
        return true
    end
    if whenv and whenv:object_id() == me:object_id() then
        return true
    end
    return false
end

-- 玩家住宅：房间名约定 house_<门派/户籍>_<角色 id>，无住宅或加载失败回 nil
function get_my_house(who)
    if not who or who:querystr("resident") == "" then
        return nil
    end
    local room = load("room", "house_" .. who:querystr("resident") .. "_" .. who:id(1))
    if not room or room:query("fail_ob") ~= 0 then
        return nil
    end
    return room
end

-- 最低武功等级：query_skills 映射取最小，无技能回 0
function index.mini_skill(ob)
    local skills = ob:query_skills()
    local lowest = nil
    for _, lv in pairs(skills or {}) do
        if not lowest or lv < lowest then
            lowest = lv
        end
    end
    return lowest or 0
end

-- 清空条件：持久与临时 condition 整键删除（键值即条件族的唯一存放面）
function index.clear_condition(ob)
    ob:del("condition")
    ob:del("condition", "T")
end

-- 可用经验：combat_exp - 升级所需（level2exp 与引擎 level2_exp 同式同源）- 已用 sk_exp
function index.query_valid_exp(ob)
    return ob:attr("combat_exp") - level2exp(ob:query("level")) - ob:query("sk_exp")
end
