-- hand.lua  基本手法

return {
    create = function(self)
        self:set_name("基本手法")
    end,

    query_action = function(self, me)
        return { action = "$N一巴掌打在$n的脸上。" }
    end,

    valid_learn = function(self, me)
        local who = me:environment():present(me:querystr("family/master_id"))
        if who and who:query("add_skill/count") ~= 0 then
            return notify_fail("你的师傅暂时没空指点你武功。。")
        end

        local who1 = me:environment():present(me:querystr("couple/id"))
        if who1 and who1:query("add_skill/count") ~= 0 then
            return notify_fail("你暂时无法指点你伴侣的武功。。")
        end

        return 1
    end,
}
