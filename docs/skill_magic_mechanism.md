# 技能与法术（magic）机制说明

> 面向数据集脚本作者与引擎维护者。本文说明**法术怎么注册**、**法术怎么被找到**、
> **法术由谁的脚本执行**、**施法的资源与状态结算**，以及若干必须遵守的生命周期不变量。
> 每处标注源码位置，便于核对。

---

## 一、一句话概括

> **法术在「技能创建时」注册进技能自己的法术表；高级技能会继承基础技能的法术；
> 施法时按「归属技能」转发执行 —— 继承来的法术，逻辑跑在基础技能的脚本里。**

---

## 二、三个概念先分清

| 概念 | 含义 | 存放位置 |
|:---|:---|:---|
| **技能种类（usage）** | 22 个固定种类：`throwing` / `force` / `unarmed` / `sword` / `parry` / … | `CCmdStandard::enable`（`src/cmd/CmdStandardMaster.cpp:587`）、`CLuaSkillT::setup_base_skill`（`src/lua/LuaSkill.inl:58`） |
| **基础技能** | 与种类同名的技能，即"这一类的默认技能"：`throwing` / `force` / … | 脚本 `lua-default/skill/basic/<种类>.lua` |
| **高级技能** | 玩家用 `enable <种类> <高级技能>` 顶替某一种类的高级技能：`throw_adv` / `force_adv` / … | 脚本 `lua-sample/skill/<高级技能>.lua` |

角色侧的映射记在 `CChar::skill_map`（`src/char/CharSkill.cpp:127`）：

```
skill_map["throwing"] = "throw_adv"     // enable throwing throw_adv 之后
```

**要点：技能对象是全局单例**（`g_skill_list`，`src/skill/SkillMgr.cpp:17`），
映射是**角色级**的。所以"某技能有哪些法术"是技能自身的属性，与角色无关；
`enable` 只决定**用哪个技能**。

---

## 三、注册期：法术怎么进表

### 3.1 脚本自注册

技能脚本在 `create` 里用 `self:add_magic(...)` 登记法术：

```lua
-- lua-default/skill/basic/throwing.lua
create = function(self)
    self:set_name("基本投掷")
    self:add_magic("sneak", "偷袭", 30, 100, CAST_TO_OTHER, CAST_IN_BOTH, "do_sneak")
end
```

签名（`src/skill/Skill.cpp:247` 的 `l_add_magic`）：

```
add_magic(名称, 显示名, mp, ap, 目标, 施法时机, Lua函数名 [, hostile])
          ^^^^^  ^^^^^^  ^  ^  ^^^^  ^^^^^^^^  ^^^^^^^^
          法术标识 显示名 内力 行动点  CAST_TO_OTHER  CAST_IN_BOTH  do_sneak
```

| 参数 | 含义 | 缺省 |
|:---|:---|:---|
| 名称 | 法术标识（`cast` 的第二个参数，如 `sneak`） | 必填 |
| 显示名 | 下发给客户端的名字（`DOWNMAGIC` 的 `N=`） | 必填 |
| mp | 内力消耗（`DOWNMAGIC` 的 `R=`） | 10 |
| ap | 行动点基准值（`DOWNMAGIC` 的 `A=`） | 100 |
| 目标 | `CAST_TO_OTHER` / `CAST_TO_ME` / `CAST_TO_NONE` … | `CAST_TO_RANDOM` |
| 施法时机 | `CAST_IN_NONE` / `CAST_IN_COMMON` / `CAST_IN_COMBAT` / `CAST_IN_BOTH` | `CAST_IN_NONE` |
| Lua函数名 | 本脚本里的函数名，**施法时调用它** | 必填 |
| hostile | 是否敌意行为（决定 PK 标记，1=敌意） | 1 |

> ⚠️ `hostile` 漏传会默认成 **1（敌意）**。自身调理类法术（复元术一类）要显式传 `0`，
> 否则会误挂 PK 标记。

### 3.2 继承：基础技能的法术登记到高级技能上

高级技能声明自己能顶替哪些种类（`valid_enable`），引擎据此决定继承谁：

```lua
-- lua-sample/skill/throw_adv.lua
base = "throwing",                            -- 选 C++ 基类（见 §3.4）
valid_enable = function(self, usage)
    return (usage == "throwing") and 1 or 0   -- 我能当"投掷"用
end,
```

技能创建收尾时（`src/lua/LuaSkill.inl:113` `inherit_base_magics`）：

```
对每个 valid_enable 为真的种类 U：
    U 即基础技能名 → load_skill("throwing") → 把它的法术表拷进本技能
```

拷贝时每条继承项都记上**归属**（`Magic_t::owner` 指向基础技能），
`lua_func` 也一并带过来（见 §4.2）。

| 规则 | 说明 |
|:---|:---|
| **同名以自身为准** | 本技能自己注册过的法术不会被继承项覆盖（`inherit_magics`，`src/skill/Skill.cpp:71`） |
| **基础技能没有法术 → 不产生幽灵条目** | 基础技能表为空时，本技能表照旧为空 |
| **只继承存在的脚本** | `script_exists` 过滤，避免为不存在的种类造出空技能 |
| **防循环** | 创建链上的技能名记在静态集合里（`s_creating_skills`），A 继承 B、B 又继承 A 时不再往下走 |

### 3.3 结果长什么样

以 `enable throwing throw_adv` 为例：

| 技能 | 法术表 | 归属 |
|:---|:---|:---|
| 基本投掷 `throwing` | `sneak` | 自身 |
| 高级投掷 `throw_adv` | `feixing`（自注册）+ **`sneak`（继承）** | `feixing`→自身，`sneak`→基本投掷 |

所以**高级技能的法术清单里含基础技能的法术** —— 这正是客户端技能面板要显示的内容
（`DOWNMAGIC` 逐条下发，`src/skill/Skill.cpp:98`）。

### 3.4 脚本里没有 `base` 字段

技能**没有族基类**了：所有技能都是同一个实现类 `CLuaSkill`（`src/lua/LuaSkill.h`），
行为全在 Lua 脚本里。因此：

- 脚本里**不需要**（也不该有）`base = "force"` / `base = "throwing"` 字段 ——
  它曾经用来选 C++ 基类（`CForce` / `CThrowing`），那两个类已随实现下沉而删除。
  引擎不读这个字段，数据集里留着也无害。
- **"属于哪一族"由 `valid_enable` 声明**（§3.2）：能顶替哪些技能种类，
  就继承哪些种类的基础技能 —— 族标识与继承来源是同一件事，不需要两个字段。

> C++ 侧只剩两层：`CSkill`（法术表与施法流程的骨架）+ `CLuaSkill`（脚本分派）。
> 内功反震在 `lua-default/skill/basic/force.lua`，出招姿势在
> `lua-default/skill/basic/throwing.lua`，暗器查找是
> `get_first_throwing_weapon()`（`src/combat/WeaponEffect.h`，与投掷消耗同处）。

---

## 四、使用期：施法怎么走

### 4.1 `cast` 命令的解析链

```
cast <种类名> <法术名>
     │      └─ 法术标识（如 sneak / recover / feixing）
     └──────── 技能种类（throwing / force / …），也就是基础技能名
```

> `cast` 的第一个参数是**种类名**，不是高级技能名。
> `cast throw_adv feixing` 走不通：映射是按种类名建的，
> `query_skill_mapped("throw_adv")` 查不到 → 报"请先准备技能"。

`src/cmd/CmdStandardCombat.cpp:59` 的解析顺序：

```
1. 角色是否学会该种类            query_skill(arg, 1) < 1 → "你还不会这种技能！"
2. 该技能是否 special            special 技能免映射，直接用
3. 查角色映射                    mapped = query_skill_mapped(arg)
                                 mapped 为空 → "请先准备技能。"
4. 武器校验                      按种类判断徒手/持械
5. 施法                          sk->cast(magic, me, who)
```

`enable` 决定第 3 步查到谁：映射到高级技能就用高级技能，否则用基础技能。

### 4.2 归属转发：谁执行这个法术

法术条目里存的是 **Lua 函数名**，不是函数本身。执行时要把这个名字回到**某个脚本的
table** 里取函数 —— 而继承来的 `sneak`，`do_sneak` 在 `throwing.lua` 里，
**不在** `throw_adv.lua` 里。

所以施法分派（`src/lua/LuaSkill.inl:292`）按归属走：

| 法术来源 | 执行者 | 例子 |
|:---|:---|:---|
| 自身注册 | 本技能脚本 | `feixing` → `throw_adv.lua` 的 `do_feixing` |
| 继承 | **归属技能**（基础技能）的脚本 | `sneak` → `throwing.lua` 的 `do_sneak` |

```
CLuaSkillT::cast(magic, me, target)
  ├─ 法术未注册                      → 0（"不支持该法术"）
  ├─ 归属不是自己                    → 转发给归属技能重新 cast（先过本技能的施法前置）
  ├─ lua_func 为空                  → 0（没有实现）
  └─ 本脚本的函数                    → pcall 执行
```

`CSkill::cast`（`src/skill/Skill.cpp:131`）只保留"转发"这一件事：
C++ 侧**不实现法术**，法术一律由 Lua 脚本提供。

### 4.3 脚本回调的回落：与法术继承同构

有些能力是"某一族技能共有"的（如内功反震、出招姿势）。它们由**基础技能的脚本**实现，
高级技能没写自己的就继承基础技能的 —— 与法术继承同一套思路：

| 回调 | 脚本里有 | 脚本里没有 | 归属技能的例子 |
|:---|:---|:---|:---|
| `hit_ob` | 调脚本的 | 回落**基础技能** | 高级内功 `force_adv` → 基本内功的内功反震 |
| `query_action` | 调脚本的 | 回落**基础技能** | 高级投掷（没写姿势时）→ 基本投掷的出招姿势 |
| 其余（`valid_learn` / `practice_skill` / `query_dodge_msg` / `query_parry_msg` …） | 调脚本的 | 回落 **C++ 基类默认实现** | — |

```
CLuaSkillT::hit_ob(me, victim, damage)
  ├─ 脚本里有 hit_ob            → 调脚本的（返回 damage_bonus 与可选消息）
  └─ 脚本里没有                 → 回落到「基础技能」的 hit_ob
                                 （高级内功 force_adv 没有自己的 → 用基本内功的内功反震）
```

- 回落只走一层，且只回落到**脚本里确实实现了该回调**的基础技能
  （`CSkill::has_hit_ob()` / `CSkill::has_query_action()`，`src/skill/Skill.h:70`）
  —— 避免继承链成环时递归不止。
- 脚本加载失败也走回落（与其他回调一致）。
- 基础技能也没有时，最后回落到 C++ 基类的默认实现（`CSkill` 为空）。

> 这两族的实现都只有一份：
> 内功反震在 `lua-default/skill/basic/force.lua` 的 `hit_ob`，
> 投掷姿势在 `lua-default/skill/basic/throwing.lua` 的 `query_action`。
> C++ 的 `CForce::hit_ob` / `CThrowing::query_action` 均已删除。

### 4.4 施法的前置与结算

顺序（任一步失败即中止，且**不扣任何资源**）：

| 步骤 | 检查 | 失败提示 |
|:---|:---|:---|
| 1 | 施法内功前置 `check_cast_force` | `valid_force` / `valid_force_pro` / `hooks.char_rule("cast_force")` |
| 2 | 行动点 `check_ap` | "需要xx点AP，你的AP不足。"（仅战斗中） |
| 3 | 执行脚本函数 | 返回 0 表示失败 |

成功后：

| 结算 | 说明 |
|:---|:---|
| 行动点扣减 | `ap = 基准值 × (120 − 技能等级/4 − 角色等级/2) / 100`，战斗中扣 |
| 施法冷却 | `no_cast` 临时标志 + `call_out` 清除 |
| 施法者状态 | `me->send_state()` |
| **受术者状态** | 目标类型为 `CAST_TO_OTHER` 时，给受术者也发一份状态（法术可能改了它的 hp/mp） |
| 施法收益 | `improve_skill` 记在**归属技能**上（继承来的 `sneak` 提升基本投掷 —— 它的等级才是命中判定的依据，与近战提升 `attack_type` 同语义） |
| PK 判定 | 按归属技能的 `hostile` 标志决定是否挂 PK 标记 |

> 受术者在施法过程中被销毁（如偷袭致死）时，事后不再触碰它
> （`g_last_destruct_object` 判定）。

---

## 五、生命周期不变量（改代码前必读）

> **从技能缓存里摘除技能，必须走 `skill_release_one(name)`，不能直接 `delete`。**

原因：继承项的 `owner` 指向基础技能。基础技能被销毁后，若继承者还留在缓存里，
它的继承项就指向了已释放的对象 —— 施法转发时会踩到野指针。

`skill_release_one`（`src/skill/SkillMgr.cpp:63`）会**连带摘除继承者**
（传递闭包：继承自它的、以及继承自那些的），下次 `load_skill` 时重建并重新继承。

| 场景 | 正确做法 |
|:---|:---|
| 脚本热重载（`reload <脚本名>`） | `skill_release_one(name)`（`src/lua/LuaEngine.cpp:1324` 已如此） |
| 全部重载（`reload all`） | `skill_release()` 一次全清（整体销毁，归属指针不会悬垂） |
| 测试里清理全局技能缓存 | `skill_release_one(name)` |

新增"从缓存摘除技能"的代码路径时，**必须**用 `skill_release_one`。

---

## 六、协议

| 封包 | 时机 | 关键字段 |
|:---|:---|:---|
| `DOWNSKILL`（C=16） | 技能详情下行 | `S=` 脚本名，`N=` 技能名，`E=` 可 enable 的种类列表（逗号分隔），`P=` 图标 |
| `DOWNMAGIC`（C=28） | 每个法术一条 | `S=` 所属技能，`M=` 法术标识，`N=` 显示名，`R=` mp，`A=` ap，`T=` 目标类型，`V=` 施法时机 |

客户端（`cli/src/game_state.cc:463`）按 `S=` 把法术归到对应技能下，
按 `M=` 幂等合并（法术会随技能全量重推反复下发）。

---

## 七、数据集写一个新技能

```lua
-- lua-sample/skill/my_throwing.lua
return {
    valid_enable = function(self, usage)       -- 声明能顶替的种类
        return (usage == "throwing") and 1 or 0
    end,

    create = function(self)
        self:set_name("我的高级投掷")
        -- 自己注册法术：ap=80，敌意（hostile 缺省 1）
        self:add_magic("my_tech", "绝技", 80, 80, CAST_TO_OTHER, CAST_IN_COMBAT, "do_my_tech")
    end,

    do_my_tech = function(self, me, target)    -- 实现
        ...
        return 1                                 -- 返回 0 表示施放失败
    end,
}
```

要点：

1. **法术实现只写一份在 Lua 里**；C++ 侧不再有法术实现，也不再有族基类
   （脚本里不需要 `base` 字段，§3.4）。
2. 声明了 `valid_enable` 就会**自动继承**基础技能的法术（如基本投掷的 `sneak`），
   不需要、也不应该手工调用基类 `create`（技能侧没有这个钩子）。
3. `add_magic` 的 `hostile` 要显式写：自身调理类传 `0`。
4. 脚本文件名 = 数据键 = `base_name`（`lua-default/skill/basic/throwing.lua` 的键是 `throwing`）。
5. 内功这一族还要注意 `hit_ob`、投掷一族要注意 `query_action`：脚本没写就自动继承
   基础技能的实现；要**自己改**行为，就在脚本里写同名函数。
6. 脚本要自己 `create`（设名字、注册法术）；缺 `create` 时技能名回落为脚本名，
   便于定位是哪个脚本不完整。

---

## 八、测试锚点

| 文件 | 锁定的契约 |
|:---|:---|
| `tests/unit/test_skill_magic_inherit.cpp` | 继承登记、归属转发、同名以自身为准、无幽灵法术、基础技能摘除时继承者连带重建 |
| `tests/unit/test_skill_cast.cpp` | AP 闸门与扣减、受术者状态刷新（敌意/非敌意/无目标三种） |
| `tests/unit/test_skill_throwing.cpp` | 偷袭的六道守卫与两种结局（全部经 `cast` 真实施法路径）；出招姿势与高级投掷的姿势继承 |
| `tests/unit/test_force_adv_recover.cpp` | 复元术（继承自基本内功）的可用性与缩比治疗；内功反震（扣蓝/夹取/反震四档播报）；高级内功继承基本内功的反震 |
| `tests/unit/test_cmd_standard_combat.cpp` | `cast` 端到端：enable 高级技能后基础法术可用、未 enable 仍是"请先准备技能" |

> 用例一律经**施放法术 / 脚本回调**触发，不再直接调技能函数 ——
> 这样脚本里漏掉 `add_magic` 登记、或回调实现写错时，测试会直接失败。

---

## 九、常见坑

| 现象 | 原因 | 处理 |
|:---|:---|:---|
| 高级技能放不出基础技能的法术 | 技能没声明 `valid_enable`（不继承），或脚本没 `add_magic` | 补 `valid_enable` / `add_magic` |
| 法术"找不到"但没报错 | 继承项的 `lua_func` 在本脚本里不存在 | 检查基础技能脚本是否真的实现了该函数 |
| 自身调理法术误挂 PK 标记 | `add_magic` 漏传 `hostile=0` | 显式传 `0` |
| `cast <高级技能> <法术>` 报"请先准备技能" | 第一个参数必须是**种类名** | `cast throwing feixing` |
| 客户端技能面板没有继承来的法术 | 高级技能没声明 `valid_enable` | 补声明 |
| 改了基础技能脚本，高级技能没跟着变 | 需触发重载（`reload`）让技能重建并重新继承 | `reload <脚本名>` |
| 内功反震毫无动静（伤害加成为 0） | 脚本里引用了**不存在的常量**（如凭空写一个 `IS_XXX`）→ `me:Query(nil)` 抛错，回调每次都在那一行中断 | 删掉不存在的常量；`hit_ob` 的实现只留一份在 Lua |
| 高级内功没有反震 | 脚本没写 `hit_ob` 且没声明 `valid_enable`（无从判断基础技能） | 声明 `valid_enable("force")` |
| 高级技能没有出招姿势，近战中止 | 脚本没写 `query_action` 且没声明 `valid_enable` | 声明 `valid_enable("throwing")`，或自己写 `query_action` |
| Lua 回调静默失败 | 回调里报错只记日志（`Lua skill hit_ob failed [...]`），调用方拿到空结果 | 看日志里的 Lua 报错；用单测经脚本回调触发，不要直调 C++ 函数 |
