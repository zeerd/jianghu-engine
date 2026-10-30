-- mirror_base.lua 镜像NPC 通用逻辑（简版）
--
-- 镜像NPC 不是引擎类型：普通脚本 NPC 自己 require 本模块，并在文件
-- 末尾调用 install 把缺省钩子（die / chat_msg / chat_msg_combat /
-- reward_killer）装进自己的脚本表——脚本自定义的同名钩子优先
-- （install 只补齐缺失项），故必须在全部脚本定义之后调用；
-- make_mirror 不进脚本表，调用方经模块引用调用（见其自身注释）。
--
-- 镜像NPC 的本分：照一位玩家抄出「另一个他」——make_mirror 把挑战者的
-- 技能、技能映射、激发、先天属性、气血内力与历练等级按 diff / 10 抄到
-- 自己身上，供任务与对练拿来回合过招。
--
-- 本文件是简版：只引用 lua-default（与 lua-sample）真实存在的技能。
-- 完整原版（泰山剑/逍遥游/法术基线、木刺配装、吃药自救、烈焰绝招）在
-- jianghu-dataset 的 npc/mirror_base.lua——同名模块按 package.path 的
-- 脚本根优先级解析：dataset 世界自动取原版，default 世界取本简版。
--
-- 相对原版的取舍：
--   1. 基线技能只用基本技能（sword/force/dodge/parry/unarmed），没有
--      专属高级线可用时，抄来的玩家技能就是镜像的全部身手；
--   2. 不配武器：简版世界没有可加载的武器物品，镜像空手迎战；
--   3. 低血自救只保留「尝试 heal perform」——法术由技能脚本提供
--      （如 lua-sample 的 force_adv），世界没提供时静默失败；
--      吃药与绝招分支随之取消（那些物品/技能只在 dataset 存在）。

local mirror_base = {}

-- 基线技能等级：10 + 玩家等级 * 3，封顶 400
-- （镜像的保底身手：玩家技能太烂时它至少还有这一份）
local function base_level(me)
    local lvl = 10 + me:query("level") * 3
    if lvl > 400 then
        -- 没必要让这个NPC成为高手的克星
        lvl = 400
    end
    return lvl
end

-- 按倍率抄一位玩家到 self 身上（diff 取 1~20，实际倍率 = diff / 10）。
-- 不是引擎派发的钩子：调用方经模块引用调用
--   local mirror_base = require("npc.mirror_base")
--   mirror_base.make_mirror(self, me, diff)
-- （self: 只认引擎绑定方法，脚本函数必须借模块引用调用）。
-- 也不设置名字：抄完的名字留空是原样，脚本必须自行 set_name，
-- 否则 /t 与房间识别从此认不出它。
function mirror_base.make_mirror(self, me, diff)
    local lvl = base_level(me)

    -- 先设置基本技能：抄来的玩家技能按同名覆盖等级，没抄到的保底留用
    self:set_skill("sword", lvl)
    self:set_skill("dodge", lvl)
    self:set_skill("parry", lvl)
    self:set_skill("force", lvl)
    self:set_skill("unarmed", lvl)

    self:map_skill("sword", "sword")

    -- 复制玩家技能等级
    local skills = me:query_skills()
    if skills then
        for name, level in pairs(skills) do
            self:set_skill(name, level * diff // 10)
        end
    end

    -- 复制玩家技能映射（enable 关系）
    local mappings = me:query_skill_map_list()
    if mappings then
        for _, item in ipairs(mappings) do
            self:map_skill(item.usage, item.mapped)
        end
    end

    -- 复制玩家激发（备战斗招式）
    local prepares = me:query_skill_prepare_list()
    if prepares then
        for _, item in ipairs(prepares) do
            self:prepare_skill(item.usage, item.prepared)
        end
    end

    -- 先天属性与气血内力
    self:set("str", me:query("str"))
    self:set("int", me:query("int"))
    self:set("con", me:query("con"))
    self:set("dex", me:query("dex"))

    self:set("max_hp", me:query("max_hp") * diff // 10)
    self:set("eff_hp", self:query("max_hp"))
    self:set("hp", self:query("max_hp"))
    self:set("max_mp", me:query("max_mp") * diff // 10)
    self:set("mp", self:query("max_mp") * 2)

    self:set("mp_factor", me:query("mp_factor") * diff // 10)
    self:set("combat_exp", me:query_exp() * diff // 10)
    self:set("level", me:query("level") * diff // 10)

    -- 附加属性（装备/技能带来的临时加成）
    self:set_temp("apply/str", me:query_temp("apply/str") * diff // 10)
    self:set_temp("apply/dex", me:query_temp("apply/dex") * diff // 10)
    self:set_temp("apply/attack", me:query_temp("apply/attack") * diff // 10)
    self:set_temp("apply/defense", me:query_temp("apply/defense") * diff // 10)
    self:set_temp("apply/armor", me:query_temp("apply/armor") * diff // 10)
    self:set_temp("apply/damage", me:query_temp("apply/damage") * diff // 10)
    self:set_temp("apply/dodge", me:query_temp("apply/dodge") * diff // 10)
    self:set_temp("apply/parry", me:query_temp("apply/parry") * diff // 10)

    -- 内功线：玩家没给映射时用基本内功兜底。原版在这里无条件改走专属
    -- 法术线（magic），简版没有法术线，玩家自己的内功映射原样保留——
    -- 玩家若把内功 enable 到高级内功上，镜像照用。
    if self:query_skill_mapped("force") == "" then
        self:map_skill("force", "force")
    end

    -- 检查轻功：基本轻功实在太烂（够不到基线的 1.5 倍），
    -- 就把轻功映射收回基本轻功，不再跟着玩家的烂映射走
    local limit = lvl + lvl // 2
    if self:query_skill("dodge") < limit then
        self:map_skill("dodge", "dodge")
    end

    -- 检查招架：同理，收回到与武器同一套的基本招架路数
    if self:query_skill("parry") < limit then
        self:map_skill("parry", "sword")
    end

    -- 简版不配武器：没有可加载的武器物品，空手迎战（原版会按最高
    -- 基本技击技能配一把「木刺」）
end

-- 死亡钩子的默认实现：认得凶手先发奖励，再化灰自毁。
-- reward_killer 是脚本可覆写的钩子（self: 认不到脚本函数，经 install
-- 捕获的脚本表分派）；脚本没定义时用本模块的空默认。
function mirror_base.reward_killer(self, me) end

-- 平时闲话：有概率遁走自毁（镜像的台词：抄完就跑）
function mirror_base.chat_msg(self)
    if random(10) ~= 0 then
        return nil
    end

    message_vision("$N身形晃了几晃，眨眼间就消失了！", self)
    destruct(self)
    return nil
end

-- 战斗闲话：低血自救——尝试一次 heal perform（法术由技能脚本提供，
-- 世界没提供时命令静默失败，镜像只是白出一招）
function mirror_base.chat_msg_combat(self)
    if self:query("hp") < self:query("max_hp") // 3 and random(2) == 0 then
        self:perform_action("force heal", 0)
    end

    return nil
end

-- 装配：把钩子补进脚本表（脚本自定义的同名钩子优先）。
-- make_mirror 不是引擎派发的钩子，不进脚本表——调用方经模块引用调用。
function mirror_base.install(npc)
    for name, value in pairs(mirror_base) do
        if name ~= "install" and name ~= "make_mirror" and type(value) == "function" then
            if npc[name] == nil then
                npc[name] = value
            end
        end
    end

    -- die 不能直接装默认函数：它要调脚本自己的 reward_killer，
    -- 而 self: 认不到脚本函数，只能借 install 捕获的脚本表分派，
    -- 故装的是闭包。脚本自定义 die 则整个接管（如练功木人散架）。
    if npc.die == nil then
        npc.die = function(self)
            local me = find_object(self:query_temp("last_damage_from"), 1)
            if me then
                npc.reward_killer(self, me)
            end

            message_vision("$N惨叫一声，化为灰烬。", self)
            destruct(self)
        end
    end

    return npc
end

return mirror_base
