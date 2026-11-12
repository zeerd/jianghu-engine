# 通用奖励规则（梗概未给奖励时的缺省）

## 1. 规则来源

从 dataset 中**既有剧情任务的完成点**提炼（在 NPC 的 `accept_object` /
`die` 里 grep `combat_exp` 即可定位同类代码）。完成点里能归纳出两档
代表性量级：常规主线一档、门派级大任务一档，见 §3。

既有任务的完成点都在**交付/击杀的当场内联计算**，不走公共函数——剧情链
没有共享奖励模块（`quest\quest_rewards.lua` 是商人任务专用）。
**新任务照此内联**；同一任务有多个完成出口时，把公式写成该文件内的
`local function`，不新建跨任务公共模块。

## 2. 公式

```lua
-- 经验：k = 难度档位（见 §3）
local exp = math.floor(who:query("age") * who:query("level") * k)
if exp < 3000 then
    exp = 2000 + random(3000)      -- random(n) 取 0..n-1，即保底 2000~4999
end

-- 声望：随经验走，负声望玩家给负值（落井下石的既有语义，照抄）
local repute = random(math.floor(exp / 2)) + exp
if who:query("repute") < 0 then
    repute = -repute
end

who:add("combat_exp", exp)
who:add("repute", repute)
who:update_me()
```

## 3. 难度档位 k

梗概给了奖励就**照梗概**，本表只在缺省时用：

| 任务量级 | k | 依据 |
|---|---|---|
| 常规支线 / 日常挂接 | 0.87 ~ 1.5 | 既有常规任务的取值档 |
| 重要剧情主线 | 2 ~ 5 | 中间档 |
| 门派级大任务 | ≈ 10 | 既有门派大任务的取值档 |

拿不准取 1.5。同一任务只取一个 k，写在完成点常量处并注一句设计理由
（量级判断），不写测试或文档依据。

## 4. 额外奖励（门派归属分支）

既有任务都在完成点判门派，给本门弟子额外宝物：

```lua
if who:querystr("family/family_name") ~= "<相关门派>" then
    who:add("combat_exp", exp)
    -- 常规播报
else
    local bot = load("item", "<宝物 base_name>")
    if bot then
        bot:move(who)
        -- 或限时宝物：bot:set("owner", who:id(1)); bot:set("time", 24)
    end
    who:add("combat_exp", exp)
    -- 门派专属播报（宝物名入句）
end
```

- 宝物用 dataset 里**已有的** item：按门派 grep 该派 NPC 发放过的物品
  作为候选，不为奖励新建物品文件；确需新宝物走"小量添加"预算并说明理由。
- 梗概只说"给件宝物"没点名 → 从候选里选，或给通用消耗品；
  选型写进交付说明。

## 5. 播报

完成点固定三层，文本里**带确切数值与专名**（推进与统计都靠它）：

```lua
-- 场景可见
message("vision", string.format("$HIR$N获得了%d点实战经验，同时得到了%d点声望。", exp, repute), who)
-- 全服（完成时刻一条即可，别连发）
do_channel(self, nil, "chat", string.format("$HIC%s完成了「%s」，获得$HIY%d$HIC点经验、$HIY%d$HIC点声望。$COM", who:name(1), "<任务名>", exp, repute))
-- 门派级成就再加一条 sys
do_channel(self, nil, "sys", string.format("%s(%s)完成了「%s」。", who:name(1), who:id(1), "<任务名>"))
```

播报里的任务名与关键数值保持**固定措辞**——它是任务推进消息的
候选匹配点（`script-model.md` §4）。

## 6. 接取间隔与防重复领奖（必须）

接取间隔是梗概的输入项（确认清单必问，缺省拟议"每岁一次"）。
两件事要同时成立：**同一次完成不重复发奖**、**下次可接取时间按档位闸住**。
按确认清单的档位选模板：

### 档 A：每岁一次（缺省）

```lua
-- 完成点（持久水位 + 完成标记）
who:set("<前缀>/jq", who:query("age"))
who:set("<前缀>/done", 1)

-- 接任务点（入口闸门）
if who:query("<前缀>/jq") == who:query("age") then
    return "你今年已经做过这件事了。"
end
```

- 水位用 `age`（岁）而非日期：跨天不误伤、重登不重置（既有任务的取法）。

### 档 B：每天 / 每 X 分钟

用秒级时间戳 `current_time()`（dataset 通用，秒为单位）：

```lua
-- 完成点
who:set("<前缀>/last", current_time())

-- 接任务点：INTERVAL 为秒数（86400 = 一天，1800 = 30 分钟）
local INTERVAL = 86400
local last = who:query("<前缀>/last")
if last ~= 0 and current_time() - last < INTERVAL then
    local left = INTERVAL - (current_time() - last)
    return string.format("这事还得再等%d分钟才能接。", (left + 59) // 60)
end
```

### 档 C：一次性

完成标记即闸门，完成点写持久 `<前缀>/done`，接任务点判
`who:query("<前缀>/done") ~= 0` 则拒。

### 通用约束

- 推进中的段键用 `"T"` 临时袋，**水位/时间戳/完成标记必须持久**。
- 放弃任务：`who:del("<前缀>")` 清整棵子树——时间戳与完成标记若也在
  前缀树内会被一并清掉，**不想被放弃重置的键要放在树外**
  （如 `jq_last` 这类独立键名），按语义决定归属并在设计稿里写明。
- 档位、间隔数值与"为什么选这档"写进交付说明。

## 7. 可选：门派日常挂接

任务若由门派派单（`npc\class\zushi.lua`），完成点推进步进：

```lua
if who:query("daily/family/task/daily/step") == <本任务 step> then
    who:set("daily/family/task/daily/step", <step + 2>)
end
```

`step % 10 == 2` 的回执发经验+门派贡献由派单方统一处理——挂接只改
step，**不要在完成点重复发日常奖励**。梗概没提门派日常就不挂。

## 8. 自查

- [ ] 保底、声望式、负声望取负三件套齐全，k 有明确档位理由。
- [ ] 接取间隔档位与确认清单一致，入口闸门与完成点写在同一套键上。
- [ ] 水位键持久 + 入口闸门，重复交付/重复击杀都不再发奖。
- [ ] 播报含任务名与确切数值，场景/全服层次分明不刷屏。
- [ ] 额外宝物来自既有 item，或走预算新建并说明。
- [ ] 完成点内联（或本文件 local function），未新建跨任务公共模块。
