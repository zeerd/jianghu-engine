# equip_temp_attr.sce — 穿戴类装备的临时属性：佩戴前后各查一遍属性
#
# 覆盖：CEquip::do_wield / do_un_wield 的 apply/* 加撤链路（穿后 owner 加
#   apply/<键>，卸下反向扣回），以及 CCmdStandard::wield/un_wield 穿卸后的
#   flush_my_info 属性推送。
#
# 排序：紧接 buy_sell.sce（钱货两讫）、必须在 wield_sword.sce **之前**——
#   此刻手槽全空、本道未学任何带四维加成的技能
#   （attr() = dbase + apply/* + unarmed/force/literate/dodge 各 /10，
#   本道只被巫师 set 过 parry/parry_adv，不进四维合成），四维基线即注册值。
#
# 判定纪律（属性断言全部走状态，不看回执文案）：
#   · 每件：穿前基线 == 注册值 → 穿后**目标键 +N 且其余三维纹丝不动**
#     （交叉键位验证——apply/con 落错成 dex 时目标断言与交叉断言双红）
#     → wait wield 断落槽 → 卸下复原；
#   · 数值字面量 = $Lanes L1 Attrs（24 20 21 19）+ 各件 apply 修正，
#     改 L1 四维必须同步本段字面量；
#   · 青巾/皮胸甲/布鞋/玉项链四件归 L1 独占采购（design §2.4），商人序号
#     19~22（carry_good 队尾追加，前面序号不动）。
#
# 玉项链（apply/per）单独口径：per 不在 RTNINFO 字段里，客户端无数值通道，
#   走外貌文案反推——rank.lua 的 waimao 按 per 精确取表（男性 11~39 逐值
#   互异），per 变则文案必变。穿前 look 抓基线句，穿后再 look 后用 notext
#   钉住「基线句不得再出现」：apply/per 没生效时文案不变，基线句重现即红。

# --- 青巾（apply/int +2 → int 20→22，头部槽） -----------------------------
cmd /t huo ji
clear
cmd /buy 19
wait text 你从杂货老板那里买下了数量1的青巾 timeout=10000
cmd /t 0
wait attr str == 24 timeout=10000
wait attr int == 20 timeout=10000
wait attr con == 21 timeout=10000
wait attr dex == 19 timeout=10000
clear
cmd /t qing jin
cmd /wield
wait text 束起一顶 timeout=10000
wait wield 青巾 头部 timeout=10000
wait attr str == 24 timeout=10000
wait attr int == 22 timeout=10000
wait attr con == 21 timeout=10000
wait attr dex == 19 timeout=10000
clear
cmd /unwield
wait text 解下头上的 timeout=10000
wait attr int == 20 timeout=10000
cmd /t 0

# --- 皮胸甲（apply/con +3 → con 21→24，铠甲槽） ----------------------------
cmd /t huo ji
clear
cmd /buy 20
wait text 你从杂货老板那里买下了数量1的皮胸甲 timeout=10000
cmd /t 0
clear
cmd /t pi xiong jia
cmd /wield
wait text 穿上一件 timeout=10000
wait wield 皮胸甲 铠甲 timeout=10000
wait attr str == 24 timeout=10000
wait attr int == 20 timeout=10000
wait attr con == 24 timeout=10000
wait attr dex == 19 timeout=10000
clear
cmd /unwield
wait text 脱下身上的 timeout=10000
wait attr con == 21 timeout=10000
cmd /t 0

# --- 布鞋（apply/dex +4 → dex 19→23，鞋子槽） ------------------------------
cmd /t huo ji
clear
cmd /buy 21
wait text 你从杂货老板那里买下了数量1的布鞋 timeout=10000
cmd /t 0
clear
cmd /t bu xie
cmd /wield
wait text 穿上一双 timeout=10000
wait wield 布鞋 鞋子 timeout=10000
wait attr str == 24 timeout=10000
wait attr int == 20 timeout=10000
wait attr con == 21 timeout=10000
wait attr dex == 23 timeout=10000
clear
cmd /unwield
wait text 脱下脚上的 timeout=10000
wait attr dex == 19 timeout=10000
cmd /t 0

# --- 玉项链（apply/per +3，颈部槽；per 走外貌文案反推，见头注） -----------
cmd /t huo ji
clear
cmd /buy 22
wait text 你从杂货老板那里买下了数量1的玉项链 timeout=10000
cmd /t 0
# 穿前：look 自己抓基线外貌句（L1 男性、per 10~30 恒走「生得…」分支）
cmd /t $acct
clear
cmd /look
wait text 看起来约 timeout=10000
capture (生得.*?。) as waimao_before
cmd /t 0
# 穿戴：回执 + 落颈部槽
cmd /t yu xiang lian
cmd /wield
wait text 戴上一条 timeout=10000
wait wield 玉项链 颈部 timeout=10000
# 穿后再 look：外貌句必须与基线不同——notext 抓到基线句 = 文案没变 =
# apply/per 没生效。前置 wait text 先钉住新 look 输出确已到达（防 look
# 本身失败时空监视假绿）
cmd /t $acct
clear
cmd /look
wait text 看起来约 timeout=10000
wait notext $waimao_before timeout=2000
cmd /t 0
# 卸下：文案回基线（同一 notext 反向——重新 look 后基线句必须重现）
cmd /t yu xiang lian
cmd /unwield
wait text 取下颈间的 timeout=10000
cmd /t $acct
clear
cmd /look
wait text $waimao_before timeout=10000
cmd /t 0
