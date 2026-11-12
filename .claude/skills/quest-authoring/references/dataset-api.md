# dataset 任务搭建 API 速查与硬规则

> 适用范围：任务所用的 dataset 根目录（Step 3 确认的输入项，下文以
> "dataset 根目录"泛指）下的 NPC / room / item Lua。
> 与代码冲突时以代码为准；引用的行号来自引擎与 dataset 现状，可能漂移，
> 用作定位线索而非契约。引擎侧行为以绑定签名与存量用法为准。

## 1. 目录与索引

| 目录 | 内容 | 任务链落点 |
|---|---|---|
| `npc\<类>\<主题>\*.lua` | NPC，一文件一主 NPC | 推进函数全在这 |
| `room\<区域>\*.lua` | 房间，顶层 `name` = 中文地名（全局唯一） | 摆位、路线、机关 |
| `item\**\*.lua` | 物品 | 优先模板改造，少新建 |
| `quest\` | **商人任务**专用（`tradepro/*`） | 剧情链不进这里 |
| `npc\class\zushi.lua` | 门派祖师日常派单 | 可选挂接点（既有任务各占一个 step 分支，新任务另取，见 `reward-rules.md` §7） |

- **NPC `base_name` = 文件名（去 `.lua`）**，与所在子目录无关，引擎按扫描树建索引；
  同名文件撞索引。`add_object("npc", "<脚本文件名>")` 用的就是这个裸名。
- room 的 `name` 建中文地名索引，`load("room", "<房间中文名>")` 直接可用；
  `_roomcheck.ps1` 查重名与断链。

## 2. NPC 回调签名表

```lua
return {
    create        = function(self) end,                       -- 属性、注册询问；先于 setup
    setup         = function(self) end,                       -- 被接管时需 self:base_setup()
    init          = function(self, me) end,                   -- 玩家进入 NPC 所在房间时
    do_talk       = function(self, me, ask) end,              -- 返回非 0 接管；回退 self:base_do_talk(me, ask)
    handle_action = function(self, comm, me, arg) end,        -- 菜单/命令截获，非 0 终止命令链
    handle_action4= function(self, comm, me, arg, tar) end,   -- 四参版（带目标 id）
    accept_object = function(self, me, item) end,             -- 1 收 / 0 拒
    do_look       = function(self, me) end,                   -- 被观察
    die           = function(self) end,                       -- 死亡结算（战斗环节的判定主场）
    finish_kill / hit_ob / attack / accept_fight = ...,      -- 战斗链
    chat_msg      = function(self) end,                       -- 闲聊，需配 chat_chance
    chat_msg_combat= function(self) end,                      -- 战斗闲聊，需配 chat_chance_combat
}
```

## 3. 状态 API（玩家 dbase）

```lua
me:set(key, value)          -- 持久袋；me:set(key, value, "T") 临时袋（下线即失）
me:query(key)               -- 数值，缺省 0；me:query(key, "T")
me:querystr(key[, "T"])     -- 字符串，缺省 ""
me:del(key[, "T"])          -- del 沿 "/" 递归：me:del("ry") 清整棵子树
me:add(key, delta)          -- 数值累加
```

- 推进中的段状态用 `"T"`；完成标记、防重复水位用持久。
- 判定一律 `~= 0` / `== 0`（Lua 里 `0` 为真）；`querystr` 判空用 `== ""`。
- 任务道具上的标记（`set("<任务标记>", 1)`）配 `set("owner1", me:id(1))` 防串号。

## 4. 询问、对话与菜单

```lua
self:set_inquiry("话题", "字面回答")            -- 只是预置文本回答
self:set_inquiry_func("话题", "函数名")          -- 回调：调用脚本 table 里的同名函数
self:send_menu(me)                             -- 发送前必须先 add_menu_item
self:add_menu_item("显示文字", "命令", me)
```

- **回调必须用 `set_inquiry_func`**。`set_inquiry(话题, "do_xxx")` 会让玩家
  看到字面 `do_xxx`（dataset 存量有约 47 处这种写法，新建一律不学）。
- 回调函数返回**非 nil 字符串**才会 `say` 并弹菜单；返回空串只弹菜单。
- **空 `send_menu` = 无输出**（刻意用来"只说不弹"，或干脆别调）。
- 菜单命令两种落点：
  - 裸关键词（`"wen"`、`"y"` 这类短词）→ 点击后回灌为 `ask` 命令，回到 `do_talk(me, ask)`；
    引擎自动生成的询问菜单项就是这种。
  - `"$0动词 $1"` → 目标截获进 `handle_action(comm, me, arg)`；
    **必须带 `$1`**（目标槽），裸动词不进 `handle_action`。
    房间菜单同理走房间的 `handle_action`。
- 跨段/延时转移：`self:call_out("函数名", 秒, object_id())`（传函数值），
  撤销用 `self:remove_call_out("fn")`。

## 5. 战斗环节范式

**开怪**（`ask` 或 `init` 进房触发）：

```lua
message("vision", "XX冷笑一声：『打赢我再说。』", player)
player:set("<前缀>/fight", 1, "T")
self:set("owner", me:id(1))          -- 归属：奖励算给开怪人
self:kill_ob(player); player:kill_ob(self)
```

**结算**（写在 NPC `die` 里，不看"打赢"标志）：

```lua
local me = find("object", self:query("last_damage_from", "T"))
if me and me:query("<前缀>/fight", "T") ~= 0 and self:querystr("owner") == me:id(1) then
    -- 发推进消息、置完成位、发奖
end
self:die("base")                     -- 让默认死亡逻辑收尾
```

- 奖励直接进包：`local bot = load("item", "模板"); bot:set_name(...); bot:move(me)`
- 要玩家 `get`：`bot:move(self:environment())`
- 进房即战：`init(self, me)`（引擎在玩家进入房间视野时调）
- 房间门禁/副本房：`valid_leave` 无状态位则踢回；有守卫则
  `load("item", "fight_room")` 建战斗房（存量任务的房间门禁范式）

## 6. 交付环节范式（accept_object）

```lua
accept_object = function(self, who, ob)
    if ob:querystr("base_name") == "<任务物>" and who:query("<前缀>/have", "T") ~= 0 then
        destruct(ob)                 -- 收下即用必须 destruct，否则物品仍落到 NPC 手上
        who:del("<前缀>/have", "T")
        who:set("<前缀>/done", 1)    -- 或发奖
        self:say("……", who)           -- 得物/推进消息（唯一文本）
        return 1
    end
    return 0                         -- 0 = notify_fail("对方不要你的东西")
end
```

## 7. 房间与路线

```lua
-- room 文件：return { type="room", name="中文地名", create=function(self) ... end }
self:add_object("npc", "<base_name>" [, {x=,y=,dir=,active=,interval=}])
self:add_object("door", "<本房出口名>", "<目标房间名>", "<目标侧出口名>" [, dir])
self:add_object("item", "fight_room")
load("room", "<中文名>")             -- 瞬移：room_obj 可传给 npc:move / me:move
```

- 出口只由 `add_object("door", ...)` 声明构成，引擎按 door 列表生成出口，
  `_roomcheck.ps1` 校验的也正是这些引用。
- 机关/提示写在 `do_look`（观察时给菜单与台词）、`handle_action`（执行动词）、
  `valid_leave`（把关）。
- 路线可达性：相邻房间 door 互指 + `_roomcheck.ps1` 零断链。

## 8. 发奖原语

```lua
who:add("combat_exp", exp)           -- 经验
who:add("repute", repute)            -- 声望
who:update_me() / who:flush_my_info()  -- 刷新面板
tell_object(me, "文本")               -- 私发
message("vision", "文本", me)         -- 当前场景可见
message("channel", "news", "文本", me)
do_channel(self, nil, "chat|sys|news", "文本")
load("item", "<模板>"):move(me)       -- 发物品
```

公式与模板见 `reward-rules.md`。

## 9. 硬规则（写码前通读）

1. **不 `self:do_xxx()` 调脚本自定义函数**：`self` 是 C++ userdata，
   方法表里只有绑定方法，找不到就静默回退基类——菜单点了毫无反应。
   同文件用 `local function` 直调；跨文件把函数当值传。
2. **回调注册用 `set_inquiry_func`**（§4）。
3. **`send_menu` 前先 `add_menu_item`**；要"点击即生效"的菜单命令写
   `"$0动词 $1"`。
4. **出口用 `add_object("door", ...)`**，room `name` 全局唯一，
   NPC `base_name` = 文件名。
5. **状态判定 `~= 0`**，字符串判空 `== ""`。
6. **`accept_object` 收下即 `destruct(ob)`**。
7. **奖励只写在完成点**，用状态位 + 水位键防重复（同一次完成不重复发）。
8. **每段推进发唯一消息**（专名 + 固定动词），段内动作幂等。
9. **注释只写设计理由**：测试计划、剧本、覆盖率、文档编号这类依据
   不进注释；迁移记录与版本痕迹也不写。
10. **命名**：Lua `snake_case`，文件名 = 对象 `base_name`；
    常量 `UPPER_SNAKE`。
11. **编码** UTF-8 无 BOM；单文件 ≤ 500 行、单函数 ≤ 50 行，
    超了就把该任务的推进函数拆到独立文件。
12. **不给 workspace 留测试用的错误脚本**；要试就在临时目录用完即删。

## 10. 常见坑速查

| 症状 | 病因 | 动作 |
|---|---|---|
| 玩家问话题，看到 `do_xxx` 字样 | 用了 `set_inquiry` 传函数名 | 改 `set_inquiry_func` |
| 菜单点了变"未知的命令" | `self:do_xxx()` 静默失败，或命令缺 `$1` | `local function` / 补 `"$0动词 $1"` |
| `send_menu` 后没反应 | 之前没 `add_menu_item`，或菜单为空 | 先加项 |
| 物品交了却还在玩家包里 | `accept_object` 返回 1 没 `destruct` | `destruct(ob)` |
| 房间校验报 UNRESOLVED / duplicate | door 指向不存在的房间名 / room `name` 重复 | 对齐真实 `name` |
| `--check-lua` 报未定义全局 | 用了没绑定的函数或拼错 | 改用存量用法里出现过的函数 |
| 重复接任务 / 重复领奖 | 无水位键 | 持久 `age` 水位 + 完成标记 |
| 玩家卡在半路无法继续 | 段状态依赖一次性消息 | 段状态可重入：重发 ask/look 能续上 |
