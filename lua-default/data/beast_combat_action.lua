-- beast_combat_action.lua  野兽攻击动作表
--
-- key = 动词（verb），value = { action = 出招文案, damage = 伤害加成百分比 }。
-- 野兽没有技能体系，出招只认 dbase 的 verbs 白名单 + 本表：
--   - CChar::query_action 按 verbs 随机取一个 verb，到本表换文案与伤害；
--   - set_beast_verbs 的动词白名单同样以本表为唯一权威，表内没有的动词
--     即拼写错误，绑定层直接抛 Lua 错，不让它静默退化成 0 伤害默认攻击。
-- 文案占位符与技能 query_action 同一套：$N=攻击者 $n=目标 $l=目标部位。
-- damage 是加成百分比，结算时最终伤害 ×(100+damage)/100，0 表示不加成。

return {
    hoof = { action = "$N用后腿往$n的$l用力一蹬", damage = 100 },
    bite = { action = "$N扑上来张嘴往$n的$l狠狠地一咬", damage = 20 },
    claw = { action = "$N用爪子往$n的$l一抓", damage = 0 },
    poke = { action = "$N用嘴往$n的$l一啄", damage = 30 },
    knock = { action = "$N用头往$n的$l用力一撞", damage = 100 },
}
