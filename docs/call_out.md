# call_out 使用指导

**读者定位**：本文写给**懂编程、但完全没学过 Lua 或只懂一点皮毛**的人。

本规范只有**一套写法**：效果对自己起作用、对目标起作用，目标是玩家还是 NPC，
写法**完全相同**。

## 一条写法

> **延迟函数写在发起 `call_out` 的脚本文件里；注册与取消写在「效果作用的对象」上。**

- 效果对自己起作用 → 对象是 `self`
- 效果对目标起作用 → 对象是目标（下例中的 `me`）；**目标是玩家还是 NPC，同一份代码原样可用**

效果不需要提前结束时，省略 `remove_call_out` 那一行即可。

## 读例子前的 Lua 说明（初学者必读）

1. **脚本文件的骨架**：开头 `local npc = {}`，函数一律写成 `function npc.名字(...)`
   （挂在模块上），结尾 `return npc`。延迟函数同样写成 `模块名.名字` 的形式
   （如 `npc.recover`）。
2. **`self` 与冒号**：`self:call_out(...)` 是固定写法，意思是「以 self 为对象执行
   call_out」。延迟函数的第一个参数固定写 `self`——引擎调用它时，`self` 就是注册时
   的那个对象（自己或目标），效果就写在它身上。
3. **`local`**：只在本函数内有效的变量。
4. **`~=` 就是「不等于」**。注意 **Lua 里 0 是「真」**：`if x then` 在 x 为 0 时也会
   进分支——判断「有值且不为 0」写 `if x ~= 0 then`。
5. **`return 0` / `return 1`**：命令处理函数里，0 = 这条命令不归我管（交给别处），
   1 = 已处理完毕。
6. **`create`、`handle_action` 是引擎会自动调用的入口函数**。
7. **延迟函数参数 `p1, p2`**：来自 `call_out` 的第 3、4 个参数（只能是整数），
   没传就是 0；用不到就不要写这两个参数。

## 例子一：效果对自己起作用（定时回血）

```lua
-- npc/sample_healer.lua（自己的脚本）
local npc = {}

-- 入口函数（引擎调用）：15 拍后执行 recover
function npc.create(self)
    self:set_name("守林人", "shou lin ren")
    self:set("max_hp", 3000)
    self:set("hp", 3000)
    self:call_out("recover", 15)
end

-- 延迟函数：到点执行，self 就是 NPC 自己
function npc.recover(self)
    local hp = self:query("hp")
    local max_hp = self:query("max_hp")
    if hp < max_hp then
        self:set("hp", math.min(hp + 500, max_hp))
    end
    self:call_out("recover", 15)          -- 周期效果：延迟函数末尾再次注册自己
end

-- 提前结束：取消尚未执行的 recover
function npc.stop_recover(self)
    self:remove_call_out("recover")
end

return npc
```

## 例子二：效果对目标起作用（时效到期）

```lua
-- item/sample_amulet.lua（发起效果的脚本）
local item = {}

-- 入口函数（引擎调用）：me = 使用护符的玩家，comm = 玩家输入的命令字
function item.handle_action(self, comm, me, arg)
    if comm ~= "use" then
        return 0                          -- 不是 use 命令，不归我管
    end
    local bonus = 60                      -- 护符提供的闪避加成
    if me:query_temp("amulet/mark") ~= 0 then
        me:remove_call_out("amulet_expire") -- 重复使用：先取消上一次的到期回调
    end
    me:set_temp("amulet/mark", 1)
    me:set_temp("apply/dodge", me:query_temp("apply/dodge") + bonus)
    me:call_out("amulet_expire", 100, bonus) -- 100 拍后执行，bonus 作为 p1 传入
    tell_object(me, "护符亮了起来。")
    return 1                              -- 已处理完毕
end

-- 延迟函数：写在发起 call_out 的脚本里；执行时 self = 目标，p1 = 注册时的 bonus
function item.amulet_expire(self, p1)
    self:set_temp("apply/dodge", self:query_temp("apply/dodge") - p1)
    self:delete_temp("amulet/mark")
    tell_object(self, "护符的时效效果消失了。")
end

return item
```

## 必须遵守的规则

1. 延迟函数必须定义在**发起 `call_out` 的脚本文件**里，且写成 `模块名.名字`，
   与 `call_out` 第一个参数的字符串**逐字一致**——无论对象是自己还是目标。
2. 注册与取消必须**同一个对象、同一个名字**：自己用 `self`，目标用目标变量。
3. `call_out("名字", 拍数[, p1, p2])`：拍数 = 心跳拍（约 1 秒 1 拍）；`p1`/`p2`
   只能是整数。
4. 延迟函数是**一次性**的，执行一次后自动移除；周期效果只允许「延迟函数末尾
   再次注册自己」这一种写法。
5. 不需要提前结束的效果，不写 `remove_call_out`。
6. `call_out_combat` 写法与 `call_out` 完全相同。

---

> 本规范只对以上写法负责；规范之外的写法对错由作者自行负责。
