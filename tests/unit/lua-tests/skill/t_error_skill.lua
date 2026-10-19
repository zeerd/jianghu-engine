-- t_error_skill.lua  测试夹具：每个回调都抛错的武功
-- 供 tests/test_lua_error_paths.cpp 验证 CLuaSkillT 的报错契约。
-- create 先登记一个 Lua 法术再抛错：法术表在报错前已写入，
-- 于是 cast() 也能走到它自己的报错分支。
return {
    name = "报错武功",

    create = function(self)
        self:set_name("报错武功")
        -- add_magic(name, displayName, mp, ap, target, validCast, luaFuncName)
        self:add_magic("boom", "报错掌", 10, 100, CAST_TO_OTHER, CAST_IN_BOTH, "do_boom")
        error("create boom")
    end,

    do_boom = function(self, me, target)
        error("do_boom boom")
    end,

    query_action = function(self, me)
        error("query_action boom")
    end,

    valid_learn = function(self, me)
        error("valid_learn boom")
    end,

    valid_enable = function(self, usage)
        error("valid_enable boom")
    end,

    practice_skill = function(self, me)
        error("practice_skill boom")
    end,

    valid_force = function(self, arg)
        error("valid_force boom")
    end,

    valid_force_pro = function(self, me)
        error("valid_force_pro boom")
    end,

    valid_combine = function(self, combo)
        error("valid_combine boom")
    end,

    query_dodge_msg = function(self, me)
        error("query_dodge_msg boom")
    end,

    query_parry_msg = function(self, weapon)
        error("query_parry_msg boom")
    end,

    hit_ob = function(self, me, victim, damage)
        error("hit_ob boom")
    end,

    type = function(self)
        error("type boom")
    end,

    skill_improved = function(self, me)
        error("skill_improved boom")
    end,
}
