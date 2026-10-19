# sample_social.sce — lua-sample 非战斗剧本（生活/观察/钱庄/社交）
#
# 前置（register.sce + sample_mainline.sce 依次跑完，同一角色）：
#   主线收尾于青云山山顶（副本出口）：身上现金充足、银行存款=0、
#   已拜青云子为师（family generation=2）、气血不满（战斗后）。
#
# 覆盖的引擎能力：
#   打坐恢复（sleep_room）/ 毒药与补药对照（receive_curing/receive_healing）
#   状态（condition）生命周期（apply_condition → CLuaCondition 挂载/递减/解除）
#   NPC 观察（CChar::do_look：外貌/武功评估/eff_status_msg/同门关系）
#   Lua do_look 钩子（eff_status_msg 绑定）/ relay_say → do_emote 绑定
#   CBankOwner 存/取/余额/贷款门槛 / 传送符 / 频道年龄门槛与切换 / 社交表情
#   频道禁词（data/chanbanword）

login $acct $pass
wait login
wait room 青云山山顶
sleep 800

# ---------------------------------------------------------------------------
# 打坐恢复：半山腰 sleep_room=1；睡眠时长由引擎随机，用条件等待代替固定
# sleep；入睡/醒来均有断言，醒来 hp=eff_hp 必满（毒药数值断言的前提）
# ---------------------------------------------------------------------------
cmd /t 0
cmd /go 青云山半山腰
wait room 青云山半山腰
clear
cmd /sleep
wait text 开始睡觉 timeout=5000
wait text 一觉醒来 timeout=120000
cmd .info

# ---------------------------------------------------------------------------
# 观察人物：同门关系分支——观察师父青云子
# （family_des「是你的师父」分支 + max_hp>0 的 eff_status_msg 路径）
# ---------------------------------------------------------------------------
wait object 青云子
cmd /t 青云子
clear
cmd /look
wait text 是你的师父 timeout=10000
sleep 500
cmd /t 0

# ---------------------------------------------------------------------------
# 毒药/补药对照 + 状态（condition）生命周期
#
# 断肠散 = 即时双损伤（hp -40% 上限值、eff_hp -10% 上限值）+ 「断肠散之毒」
# 状态每拍再扣 5% 上限值的气血，合计约 45% 上限值。
#
# 状态生命周期（apply_condition → CLuaCondition::init → 心跳逐拍
# update_condition 递减 → 归零返回 0 解除）：三条播报依次命中，段内不 clear，
# 保证能捕到解除播报。
#
# 补药对照的写法随样本走，不反过来：回春丹每粒至多补 100 点，补药粒数由
# 毒伤总量决定（毒伤越深要吃的药越多），所以用「重复服用直到补满」的循环，
# 而写死「吃两粒」——毒伤一旦超过 200 点就永远补不满，断言必红。
#   金创药补的是气血上限（eff_hp）：毒伤压低上限 → 补回上限 → 再用被拒
#   回春丹补的是当前气血（hp）：补到 hp = eff_hp → 再用被拒
#   每次 use 前必须重新 /t：药品是独立单粒对象，真实生效即 destruct 本体，
#   复用旧 object id 会被 get_self_object 静默丢弃（use 直接 return 0 无输出）；
#   被拒的 use 不消耗药品（提前 return，不 destruct），故可反复试探
# ---------------------------------------------------------------------------
cmd /t 断肠散
clear
cmd /use
wait text 毒性发作了 timeout=10000
wait text 毒性渗入了你的体内 timeout=10000
wait text 毒性一阵阵发作 timeout=60000
wait text 毒性渐渐散去 timeout=90000
sleep 4500
cmd /t 金创药
clear
cmd /use
wait text 气色看起来好多了 timeout=10000
sleep 3200
repeat 5
cmd /t 回春丹
clear
cmd /use
wait text 精神立刻好了许多|不需要用回春丹 timeout=10000
sleep 3200
end
cmd /t 金创药
clear
cmd /use
wait text 不需要用金创药 timeout=10000
cmd /t 回春丹
clear
cmd /use
wait text 不需要用回春丹 timeout=10000
sleep 500

# ---------------------------------------------------------------------------
# 传送符：look 弹使用菜单 → 选目的地（半山腰 → 山脚，衔接后续生活段）
# ---------------------------------------------------------------------------
cmd /t 传送符
cmd /look
wait menu
menu 使用传送符
wait menu
menu 传送至「山脚」
wait room 青云山山脚
sleep 800
cmd .info

# ---------------------------------------------------------------------------
# 观察人物（C++ CChar::do_look）：外貌、武功评估、eff_status_msg 气血状态
# （inventory_look/add_menu_item 服务端照常执行；非尸体观察的菜单项 cmd 为空，
#   客户端丢弃空 cmd 项后空菜单自动取消，故只断言两句人物描述文本）
# ---------------------------------------------------------------------------
cmd /t 杂货老板
clear
cmd /look
wait text 武功看起来 timeout=10000
sleep 500

# ---------------------------------------------------------------------------
# 观察人物（Lua do_look 钩子）+ eff_status_msg 全局绑定
# （老李头脚本定义 do_look 后走脚本分支，不落 C++ 基座，返回 1 不弹菜单）
# ---------------------------------------------------------------------------
cmd /t 老李头
clear
cmd /look
wait text 打量了你一番 timeout=10000
sleep 500

# ---------------------------------------------------------------------------
# 说话反应：定向 say → relay_say 钩子 → do_emote 绑定（bow 作揖，
# 目标视角文案「$N对着你躬身作了个揖。」）
# ---------------------------------------------------------------------------
clear
cmd /say 你好
wait text 躬身作了个揖 timeout=10000
sleep 500
cmd /t 0

# ---------------------------------------------------------------------------
# 钱庄：余额查询/存款/取款/超额取款/贷款前置门槛
# （CBankOwner::do_balance/do_deposit/do_withdraw/do_loan）
# 主线结束时存款=0、现金>1000；贷款在有存款时优先命中「你还有存款！」
# （先于 level 门槛）。钱庄房间只连通山脚，取款后必须先回山脚再走他处。
# ---------------------------------------------------------------------------
cmd /go 山脚钱庄
wait room 山脚钱庄
wait object 钱庄老板
sleep 500
cmd /t 钱庄老板
clear
cmd /balance
wait text 你现在没有存款 timeout=10000
clear
cmd /deposit 1000
wait text 存入现款 timeout=10000
clear
cmd /balance
wait text 你现在共有存款 timeout=10000
clear
cmd /withdraw 400
wait text 提款完毕 timeout=10000
clear
cmd /balance
wait text 你现在共有存款 timeout=10000
clear
cmd /withdraw 99999999
wait text 你没这么多存款 timeout=10000
clear
cmd /loan 100
wait text 你还有存款 timeout=10000
cmd /t 0
cmd /go 青云山山脚
wait room 青云山山脚

# ---------------------------------------------------------------------------
# 频道与年龄门槛：新角色 age=14（登录初始值，游龄 24 小时一岁），
# 服务端规定非 newbie 频道一律拒绝（ChatChannel 年龄检查）。
#   纯文本默认走 chat 频道 → 断言「年纪太小」拒绝文案（年龄不够的真实处理）
#   .tune new 是客户端本地切频道（`.`=本地命令、`/`=服务器命令；服务端
#   tune 是「关闭接收频道」的另一语义，由 /tune 直发、客户端不走它）；
#   切换后纯文本以 new 前缀动词直发
#   发言后正向断言服务端回显「：<内容>」（回显格式【新手】<名>(<id>)：<内容>）
#   ——仅断言「未被拒」不够：发言可能被静默丢弃，无回显也无拒绝文案
# ---------------------------------------------------------------------------
clear
cmd 大家好，我是来测试的。
wait text 年纪太小 timeout=10000
sleep 600
cmd .tune new
clear
cmd 我在新手频道和大家打招呼。
wait text ：我在新手频道和大家打招呼。 timeout=10000
sleep 600

# ---------------------------------------------------------------------------
# 社交表情
# ---------------------------------------------------------------------------
cmd /face
sleep 600
cmd /t 老李头
cmd /face
sleep 600
cmd /t 0

# ---------------------------------------------------------------------------
# 频道禁词（lua-sample/data/chanbanword → CChatChannel::check_channel_msg
# → lstrchr 逐字匹配）：禁词表对所有频道生效（noise 市井频道豁免），
# 新手频道也躲不掉——含禁词的发言被服务端直接拒绝，一个字都不广播。
# 断言用拒绝文案本身：它是唯一能证明「查过禁词表」的正向回执
# ---------------------------------------------------------------------------
clear
cmd 我偏要说混蛋二字
wait text 不文明字眼 timeout=10000
sleep 600
