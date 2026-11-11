# LuaBind 绑定函数低使用量统计（调用 < 10 次）

> 统计基准：2026-11-12，对照当前工作区。
> 绑定来源：`src/lua/LuaBind.cpp` 的 `object_methods[]` + `global_funcs[]`，去重后 196 个绑定名。
> 统计范围：`lua-sample`（70 文件）、`lua-default`（49 文件）、`c:\jianghu-dataset`（5216 文件）。
> 计数口径：剥离注释与字符串字面量后，对象方法按 `x:foo(` / `x.foo(` 计，全局按独立 `foo(` 计；
> 排除 `function foo(` 定义与 `table.remove` / `math.random` 等标准库成员调用。
> `tests\`（集测剧本）单列，不计入 10 次门槛。
> 复算脚本：`tests/count_bind_usage.ps1`（入库），明细输出 `test_tmp/bind_usage_detail.txt`
> （git 忽略；`check-binds.ps1` 自带的 usage 扫描写 `test_tmp/bind_usage.txt`，两者互不覆盖）。

## 结论摘要

内容层调用 < 10 次的绑定共 **71** 个：1 次 16 个、2 次 11 个、3 次 13 个、4 次 5 个、
5 次 12 个、6 次 6 个、7 次 3 个、8 次 2 个、9 次 3 个。
按「次数 × 个数」乘积分组实施见下方「分组实施」节（共 7 组，按乘积升序组 1 → 组 7）。

## 明细（按调用次数升序）

| 次数 | 绑定名 | 类型 |
|:--:|:---|:---|
| 1 | `bind` `clear` `remove` `remove_all_box` `valid_force` | obj |
| 1 | `current_time_info` `enter_register_hall` `get_fight_team_num` `get_room_char` `get_server_id` `is_relative` `query_recruit` `random_vendor` `script_exists` `update_recruit` | global |
| 1 | `register_char` | global（仅 lua-default） |
| 2 | `clear_skill` `remove_betrayer` `set_heart_beat` `set_single` `split` `wield_ownered_item` | obj |
| 2 | `combatd_query` `get_db_type` `get_fight_team_char` `get_team_list` `paihangbang` | global |
| 3 | `base_attack` `do_active` `learn_bonus` `query_left_weapon` `random_get_item` `remove_all_killer` `reset_level` `select_member` `skill_need_points` `unlocked` | obj |
| 3 | `get_ip` `killer_reward` `setup_char` | global |
| 4 | `can_join` `is_fight_room` | obj |
| 4 | `ini_get` `is` `respect` | global（`is`/`respect` 仅 lua-default） |
| 5 | `base_reset` `begin_fb` `clear_condition` `inc_skill` `mini_skill` `move_in` `purge` `query_valid_exp` | obj |
| 5 | `db_register_ddl` `get_my_house` `make_corpse` `random_npc` | global |
| 6 | `do_action_base` `do_unwield` `load_fb_room` `query_fb_parent` | obj |
| 6 | `log` `throw_weapon` | global（`log` 仅 lua-default） |
| 7 | `call_out_combat` `is_invalid` `query_weight` | obj |
| 8 | `damge_to_string` `find_online_users` | global |
| 9 | `do_wield` `room_exits` | obj（`room_exits` 为 obj+global 合计） |
| 9 | `db_execute` | global |

分根明细（sample / default / dataset，tests 单列）见 `test_tmp/bind_usage_detail.txt`。

## 已剔除的假阳性

- `remove`：`table.remove(` 计入过 2 次，真实调用仅 `lua-default/room/resident_base.lua` 的 `house:remove()` 1 次。
- `reset_level`：`jianghu-dataset/data/login.lua` 的 `local function reset_level` 是脚本本地函数，已排除。
- `valid_force`：skill 脚本自定义的同名全局函数（80 文件）不属绑定，只计 `data/hooks.lua` 的 `target:valid_force(` 1 次。

## 口径限制

- 同名的 Lua 自定义对象方法（如某 Lua 类自己的 `x:setup()`）与绑定方法不可区分，个别数字可能偏高，不会偏低。
- 动态调用 `me[name]`、以字符串注册进 hooks/commands 的调用不计入。
- 仅统计 Lua 脚本侧调用；C++ 侧内部调用不属本表范围。

## 分组实施

> 分组依据：**次数 × 个数**（该档乘积 = 档内绑定在内容层的调用点总数 ≈ 本组的改动量与回归面）。
> 分组规则：每组乘积 ≤ 45（约 40 分钟可收口的粒度）；乘积超界的 5 次档（60）按 obj / global 拆开，
> 其余次数档不拆。组按乘积升序执行，先做最便宜的组。
> 每组收口 = 组内绑定逐一裁定并落地 + `.\check-binds.ps1` 对账 OK + 单测/集测通过，留一行运行记录。

### 乘积表（按执行顺序）

> 计数为规划时快照；每组收口后在组内回填各绑定的裁定状态。

| 次数 | 个数 | 乘积 | 归组 |
|:--:|:--:|:--:|:--:|
| 9 | 3 | 27 | 组 1 |
| 1 | 16 | 16 | 组 2 |
| 8 | 2 | 16 | 组 2 |
| 6 | 6 | 36 | 组 3 |
| 3 | 13 | 39 | 组 4 |
| 4 | 5 | 20 | 组 5 |
| 5（global 部分） | 4 | 20 | 组 5 |
| 5（obj 部分） | 8 | 40 | 组 6 |
| 7 | 3 | 21 | 组 7 |
| 2 | 11 | 22 | 组 7 |
| 合计 | 71 | 257 | 7 组 |

### 组 1（乘积 27，3 个绑定）— 先做，最小的一组

- [x] 9 次 × 3 = 27
  - [x] obj：`do_wield` —— **已删（Lua 化可行）**：9 处调用点全部迁移到既有通用绑定
    `self:do_action_base(DO_WIELD)`，走同一条 C++ 路径（`CEquip::do_action` → `CEquip::do_wield`），
    零额外性能开销且多了一层 `environment()` 空指针守卫；`LuaBind.cpp` 绑定表项、
    `LuaBindDecls.h` decl、`Equip.cpp` 的 `l_do_wield`、`LuaItem.h/.inl` 的 `lua_do_wield`
    及 `call_base_do_wield`/`has_do_wield` 一并清理（`has_do_wield` 组 1 更名
    `has_do_unwield`，组 3 随 `do_unwield` 一并删除）。
  - [x] obj：`room_exits` —— **保留（Lua 化不可行，能力缺口非性能）**：出口只存在于 C++
    `m_door_list`，Lua 侧无枚举原语（`present_door` 只能按已知 id/方向查）；镜像方案立不住——
    每次 push 的 userdata 身份不唯一、房间销毁无 Lua 钩子，按 `object_id` 记账会撞 id 复用。
  - [x] global：`db_execute` —— **保留（Lua 化不可行，能力缺口）**：SQL 执行是引擎原语
    `g_db->execute`，调用点是真 DDL/DML（建表、增删改），Lua 化只能再包一层壳。
- 运行记录：2026-11-12 —— `check-binds.ps1` 四方对账 OK（绑定 196 → 195，重复/告警 0）；
  单测 2254 + cli 288 全过，覆盖率 94.7% → 94.8%、delta +0.1%；E2E 10 道 + persist/reload
  双台全绿（未登记 ERROR 0，227s/420s 预算内）；`run-cppcheck.ps1` 0 问题；
  `--check-lua` sample 70 / default 49 全 OK；dataset `validate.ps1` stylua + check-lua
  5216 全 OK（roomcheck 的 74 未解析 / 121 重名为既有状态，7 个改动文件均不在 findings 内）。

### 组 2（乘积 32，18 个绑定）

- [x] 1 次 × 16 = 16
  - [x] obj：`bind` —— **已删（可行）**：`CDoor::bind` 就是纯 dbase 写（`next_room`/
    `next_door`），调用点（yewai 出口刷新）改 `d:set("next_room", …)` + `d:set("next_door", "")`；
    `l_door_bind` 与 `CDoor::bind` 本体一并删除。
  - [x] global：`get_room_char` —— **已删（可行）**：prelude 组合 `present("object_id")` +
    `query(IS_CHAR)`，`skills.lua` 调用点不动；C++ 自由函数 `get_room_char` 保留（命令分发链在用）。
  - [x] global：`is_relative` —— **已删（可行）**：prelude 组合 师徒（同门+master_id）/
    伴侣（couple/id 互认）/ 环境互为对方（比 `object_id`）三条判定，调用点不动；
    C++ 自由函数保留（`CCmdUser::skill` 在用）。
  - [x] global：`current_time_info` —— **已删（可行）**：prelude 用 `os.date("*t")`（`os` 已
    openlibs，dataset 有大量先例），`wday` 归一到 0=周日对齐原 `tm_wday` 口径。
  - [x] global：`get_server_id` —— **已删（可行）**：调用点改 `tonumber(ini_get("server","id","1"))`
    ——与 `g_server_id = ini_get_int("server","id",1)` 同源，默认值同为 1，`ini_get` 读内存表零开销。
  - [ ] obj：`clear` —— 保留（不可行：原子清空持久 dbase，`dbase` 动作集无顶层键枚举原语）
  - [ ] obj：`remove` —— 保留（不可行：删除持久档案是 DB 原语 `db_delete_container`）
  - [ ] obj：`remove_all_box` —— 保留（不可行：`CBox::remove_objects()` 箱内清理 +
    `m_box_num` 计数器复位均无 Lua 面）
  - [ ] obj：`valid_force` —— 保留（不可行：失败文案需 `load_skill` 按 id 取中文名，无该原语；
    且实现可被技能脚本字段覆写分发，Lua 够不到他人脚本表）
  - [ ] global：`enter_register_hall` —— 保留（不可行：登录大厅是引擎流程 `enter_register_hall(me)`）
  - [ ] global：`get_fight_team_num` —— 保留（不可行：`get_team_count` 是 C++ 队伍状态；
    `get_team_list` 的 `target`/`num` 参数语义不同，替换会改变行为）
  - [ ] global：`query_recruit` —— 保留（不可行：`g_channel` 招募频道状态原语）
  - [ ] global：`update_recruit` —— 保留（不可行：同上，写通道）
  - [ ] global：`random_vendor` —— 保留（不可行：`g_item_mgr` 商人注册表无枚举原语）
  - [ ] global：`script_exists` —— 保留（不可行：引擎脚本装载器 `g_LuaEngine->script_exists`）
  - [ ] global：`register_char` —— 保留（不可行：DB 注册原语 `db_register_account`）
- [x] 8 次 × 2 = 16
  - [ ] global：`damge_to_string` —— 保留（不可行：纯格式化、性能无碍，但 prelude 分工明文
    「不复制引擎内部逻辑」——C++ 战斗播报（`Attack.cpp`）自身在用同一函数，Lua 抄一份即双源漂移）
  - [ ] global：`find_online_users` —— 保留（不可行：`g_user_list` 在线注册表无枚举原语，
    只有按账号单查的 `find("online", id)`）
- 运行记录：2026-11-12 —— `check-binds.ps1` 四方对账 OK（绑定 195 → 190，0-call 0）；
  单测 2253 + cli 288 全过，覆盖率 delta +1.4%（94.7%，`CmdUserSkill`/`Door`/`Global`
  相关路径 100%）；E2E 10 道 + persist/reload 双台全绿（未登记 ERROR 0，225s/420s 预算内，
  未命中函数 152 → 146）；`run-cppcheck.ps1` 0 问题；`--check-lua` sample 70 / default 49 全 OK；
  dataset `validate.ps1` stylua + check-lua 5216 全 OK（roomcheck 74/121 与基线持平，仍为既有状态）。

### 组 3（乘积 36，6 个绑定）

- [x] 6 次 × 6 = 36
  - [x] obj：`do_unwield` —— **已删（可行）**：与组 1 `do_wield` 同族，6 处调用点
    （gaitou/killermask/killermask1/bilinzhen/lianxindan/xblade 的 DO_UNWIELD 分支）改
    `self:do_action_base(DO_UNWIELD)`——`CEquip::do_action` 对 DO_UNWIELD 直达
    `do_un_wield()`（不多任何判定门），且多一层 `environment()` 空指针守卫；
    `l_do_unwield`、`lua_do_un_wield`、`call_base_do_unwield`、`has_do_unwield` 一并删除。
  - [ ] obj：`do_action_base` —— 保留（不可行：绕过 Lua 重写直调 C++ 基类 `do_action` 的
    引擎原语（`lua_do_action_base` 标签分派），Lua 侧无等价；已是覆写 do_action 物品的
    标准回落 API，本组迁移后调用点增至 15 处）
  - [ ] obj：`load_fb_room` —— 保留（不可行：`CFB::load_fb_room` 是副本装载原语——CFB 上下文、
    房间工厂与复用逻辑，`load("room", …)` 只装普通房）
  - [ ] obj：`query_fb_parent` —— 保留（不可行：读 `CFBRoom::m_parent_fb` 成员指针，非 dbase，
    Lua 无等价读取面）
  - [ ] global：`log` —— 保留（不可行：日志原语，prelude 的六个 `log_*` 包装全部组合在它上面）
  - [ ] global：`throw_weapon` —— 保留（不可行：`CWeaponEffect::throw_weapon` 战斗结算原语）
- 运行记录：2026-11-12 —— `check-binds.ps1` 四方对账 OK（绑定 190，0-call 0）；
  单测 2252 + cli 288 全过，覆盖率 delta +0.0%（94.7%，`Equip.cpp` 91/91 100%）；
  E2E 10 道 + persist/reload 双台全绿（未登记 ERROR 0，230s/420s 预算内，未命中 146 → 144）；
  `run-cppcheck.ps1` 0 问题；`--check-lua` sample 70 / default 49 全 OK；
  dataset `validate.ps1` stylua + check-lua 5216 全 OK（roomcheck 74/121 与基线持平）。

### 组 4（乘积 39，13 个绑定）

- [x] 3 次 × 13 = 39
  - [x] obj：`random_get_item` —— **已删（可行）**：C++ 自由函数除绑定外零调用方（删除后
    Lua 版即唯一实现，无双源）；prelude 用 `get_item_list` + `iter_items` + `query(类型)` +
    `random`（与原实现同一 RNG）组合，保留 exclude（比 `object_id`）与 50 件上限；
    3 处调用点（alms/begging/stealing）签名不变、调用点不动。
  - [ ] obj：`base_attack` —— 保留（不可行：`CChar::attack` = `g_combatd.fight` 战斗入口，
    绕过覆写直调；`combat_attack` 是回合结算，语义不同不可代）
  - [ ] obj：`do_active` —— 保留（不可行：`CUser::m_start_idle` 成员赋值，无 Lua 面）
  - [ ] obj：`learn_bonus` —— 保留（不可行：分段公式，C++ `Attack`/`CmdStandardMaster`/
    `CharFlush` 在用 → Lua 抄一份即双源漂移）
  - [ ] obj：`query_left_weapon` —— 保留（不可行：装备槽读取，装备槽无 Lua 面，同 `do_wield` 裁定）
  - [ ] obj：`remove_all_killer` —— 保留（不可行：`m_killers` 成员容器无 Lua 面）
  - [ ] obj：`reset_level` —— 保留（不可行：写流程（循环降级 + `setup_char`），
    `CReward` 击杀路径在用 C++ 实现 → 双源）
  - [ ] obj：`select_member` —— 保留（不可行：`CFightRoom` 队伍状态，同 `get_fight_team_num` 裁定）
  - [ ] obj：`skill_need_points` —— 保留（不可行：公式依赖 `learned` 点数映射（无 Lua 面），
    且 `CharFlush`/`CmdStandardMaster` 在用 → 双源）
  - [ ] obj：`unlocked` —— 保留（不可行：解锁状态机，`CBox::handle_action` 解锁链在用
    C++ 实现 → 双源）
  - [ ] global：`get_ip` —— 保留（不可行：连接层 IP，网络状态）
  - [ ] global：`killer_reward` —— 保留（不可行：`g_Reward` 奖励引擎）
  - [ ] global：`setup_char` —— 保留（不可行：角色初始化引擎流程，登录/`reset_level` 链在用）
- 运行记录：2026-11-12 —— `check-binds.ps1` 四方对账 OK（绑定 190 → 189，0-call 0）；
  单测 2252 + cli 288 全过，覆盖率 delta +0.0%（94.7%，`GameObject.cpp` 64/64 100%）；
  E2E 10 道 + persist/reload 双台全绿（未登记 ERROR 0，221s/420s 预算内，未命中 144 → 142）；
  `run-cppcheck.ps1` 0 问题；`--check-lua` sample 70 / default 49 全 OK；
  dataset `validate.ps1` stylua + check-lua 5216 全 OK（roomcheck 74/121 与基线持平）。

### 组 5（乘积 40，9 个绑定）

- [x] 4 次 × 5 = 20
  - [ ] obj：`can_join` —— 保留（不可行：`CFightRoom` 加入门槛状态，同 `select_member`/
    `get_fight_team_num` 裁定）
  - [x] obj：`is_fight_room` —— **已删（可行）**：`l_is_fight_room` 就是
    `dynamic_cast<CFightRoom*>`，与 `query(IS_FIGHTROOM)`（枚举已注册、`CFightRoom::query_attr`
    回 1）完全等价；4 处调用点（sample_arena_master ×2、xunshou_shu、qufeng）改
    `x:query(IS_FIGHTROOM) == 0` 判断。
  - [ ] global：`ini_get` —— 保留（不可行：配置读取原语——组 2 `get_server_id` 的替代即它）
  - [ ] global：`is` —— 保留（不可行：对象谓词原语，prelude 的 `is_user/is_wizard/is_living/
    is_character` 全组合在它上面）
  - [ ] global：`respect` —— 保留（不可行：称谓原语（Rank.cpp），prelude 的
    `query_respect/query_rude/…` 全组合在它上面）
- [x] 5 次档 global 部分 × 4 = 20
  - [x] global：`get_my_house` —— **已删（可行）**：纯组合——`querystr("resident")` +
    `load("room", "house_"..resident.."_"..id(1))` + `query("fail_ob")`（房间名约定与
    `resident_base.lua` 的 `get_local_house` 同源）；prelude 提供同名全局，5 处调用点不动；
    C++ 实现只在 `l_` 包装里（删除即单源）。
  - [ ] global：`db_register_ddl` —— 保留（不可行：DDL 登记原语（`g_dataset_table_ddl`，
    开机 prepare_tables 统一执行））
  - [ ] global：`make_corpse` —— 保留（不可行：尸体对象工厂（CCorpse 创建 + 入环境 + 衰变逻辑））
  - [ ] global：`random_npc` —— 保留（不可行：`g_npc_mgr` 注册表无枚举原语，同 `random_vendor` 裁定）
- 运行记录：2026-11-12 —— `check-binds.ps1` 四方对账 OK（绑定 189 → 187，0-call 0）；
  单测 2252 + cli 288 全过，覆盖率 delta +0.0%（94.7%，`RoomMgr.cpp` 22/22、
  `FightRoom.cpp` 相关路径 100%）；E2E 10 道 + persist/reload 双台全绿
  （未登记 ERROR 0，232s/420s 预算内，未命中 142 持平——两条被删绑定原本即 E2E 已命中）；
  `run-cppcheck.ps1` 0 问题；`--check-lua` sample 70 / default 49 全 OK；
  dataset `validate.ps1` stylua + check-lua 5216 全 OK（roomcheck 74/121 与基线持平）。

### 组 6（乘积 40，8 个绑定）

- [x] 5 次档 obj 部分 × 8 = 40
  - [x] obj：`mini_skill` —— **已删（可行）**：`CChar::mini_skill` 是对 skills 映射取最小，
    纯组合 `query_skills()` 即得（空表回 0）；prelude `index.mini_skill` 提供同名方法，
    5 处调用点不动；C++ 自由实现除绑定外零调用方，一并删除。
  - [x] obj：`clear_condition` —— **已删（可行）**：`CChar::clear_condition` 就是
    `del("condition")` + `delete_temp("condition")` 两个键操作，均有绑定；prelude
    `index.clear_condition` 组合，5 处调用点不动；C++ 实现除绑定外零调用方，一并删除。
  - [x] obj：`query_valid_exp` —— **已删（可行）**：`combat_exp - level2_exp(level) - sk_exp`，
    其中 `level2exp` 是 prelude 已有的与引擎同式同源的换算（既有设计），`attr`/`query` 均有绑定；
    prelude `index.query_valid_exp` 组合，5 处调用点不动（C++ `CChar::query_valid_exp`
    保留——`CChar::reset_level` 在用，两侧同式同源）。
  - [ ] obj：`base_reset` —— 保留（不可行：`CLuaRoomT::base_reset` 是绕过脚本重写直调
    C++ 基类 `reset()` 的原语，同 `base_init`/`base_setup`/`do_action_base` 一族）
  - [ ] obj：`begin_fb` —— 保留（不可行：`CFB::begin_fb` 副本开启原语——时限、参战登记与
    房间状态由引擎推进）
  - [ ] obj：`move_in` —— 保留（不可行：`CFB::move_in` 副本移入原语——队伍/位置/状态链）
  - [ ] obj：`purge` —— 保留（不可行：`CContainer::purge` 是容器清单销毁（引擎生命周期，
    `Room`/`Box`/`Container`/`ItemMgr` 四处 C++ 在用）→ 双源 + 清单手术无 Lua 面）
  - [ ] obj：`inc_skill` —— 保留（不可行：直接写 skills 映射 `(*skills)[sn] += delta`；
    `set_skill` 语义不等价（带 `load_skill` 校验 + selfcreate 技能 reset），且映射无 Lua 写面）
- 运行记录：2026-11-12 —— `check-binds.ps1` 四方对账 OK（绑定 187 → 184，0-call 0）；
  单测 2250 + cli 288 全过，覆盖率 delta +0.0%（94.7%，`CharAttr.cpp` 100%）；
  E2E 10 道 + persist/reload 双台全绿（未登记 ERROR 0，233s/420s 预算内，未命中 142 → 140，
  即本轮删除的 `l_mini_skill`/`CChar::mini_skill` 两条）；`run-cppcheck.ps1` 0 问题；
  `--check-lua` sample 70 / default 49 全 OK；dataset `validate.ps1` stylua + check-lua
  5216 全 OK（roomcheck 74/121 与基线持平）。

### 组 7（乘积 43，14 个绑定）— 最后做

- [ ] 7 次 × 3 = 21
  - [ ] obj：`call_out_combat` `is_invalid` `query_weight`
- [ ] 2 次 × 11 = 22
  - [ ] obj：`clear_skill` `remove_betrayer` `set_heart_beat` `set_single` `split`
    `wield_ownered_item`
  - [ ] global：`combatd_query` `get_db_type` `get_fight_team_char` `get_team_list` `paihangbang`
- 运行记录：

### 组间跟踪

- [x] 组 1 收口：裁定处置（删 / 下沉脚本层 / 保留）+ `.\check-binds.ps1` 对账 + 单测/集测通过
  —— 2026-11-12：`do_wield` 删（调用点迁移），`room_exits`/`db_execute` 裁定保留，门禁全过（见组 1 运行记录）。
- [x] 组 2 收口：同上 —— 2026-11-12：5 删（`bind`/`get_room_char`/`is_relative`/
  `current_time_info`/`get_server_id`），13 裁定保留，门禁全过（见组 2 运行记录）。
- [x] 组 3 收口：同上 —— 2026-11-12：`do_unwield` 删（6 调用点迁移到
  `do_action_base(DO_UNWIELD)`），`do_action_base`/`load_fb_room`/`query_fb_parent`/`log`/
  `throw_weapon` 裁定保留，门禁全过（见组 3 运行记录）。
- [x] 组 4 收口：同上 —— 2026-11-12：`random_get_item` 删（prelude 组合，C++ 自由函数
  一并删除），其余 12 个裁定保留，门禁全过（见组 4 运行记录）。
- [x] 组 5 收口：同上 —— 2026-11-12：2 删（`is_fight_room` → `query(IS_FIGHTROOM)`、
  `get_my_house` → prelude 组合），7 裁定保留，门禁全过（见组 5 运行记录）。
- [x] 组 6 收口：同上 —— 2026-11-12：3 删（`mini_skill`、`clear_condition`、
  `query_valid_exp` 均下沉 prelude），5 裁定保留，门禁全过（见组 6 运行记录）。
- [ ] 组 7 收口：同上
- [ ] 七组收口后复算 `tests\count_bind_usage.ps1`（`-Threshold` 调门槛、`-DatasetRoot` 换数据集根），确认低使用绑定已收敛；结果过期即复算。
