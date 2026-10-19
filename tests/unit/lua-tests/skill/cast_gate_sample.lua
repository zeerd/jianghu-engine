-- cast_gate_sample.lua  施法内功前置闸门样本（Lua 法术）
--
-- 用途：验证 CLuaSkillT::cast 的施法内功前置闸门对「Lua 注册法术」同样生效，
--       以及 hooks.char_rule("cast_force") 通用 hook 的放行路径。
--
-- do_probe 为纯 Lua 法术，恒返回 1（成功）。技能声明 valid_force = "force"，
-- 因此：未映射基本内功 → 闸门拦住；映射基本内功 → 放行；
--       hooks.char_rule("cast_force") 返回 {yes=true} → 未映射也放行。
return {
    create = function(self)
        self:set_name("闸门样本")
        self:set("valid_force", "force")

        -- add_magic(name, displayName, mp, ap, target, validCast, luaFuncName)
        -- 探针术：对自身施放，非敌意（不挂 PK 标记）
        self:add_magic("probe", "探针术", 10, 10, CAST_TO_ME, CAST_IN_BOTH, "do_probe", 0)
    end,

    do_probe = function(self, me, target)
        return 1
    end,
}
