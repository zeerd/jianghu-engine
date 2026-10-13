-- dodge.lua  基本躲闪

return {
    create = function(self)
        self:set_name("基本躲闪")
    end,

    query_dodge_msg = function(self, me)
        local msgs = {
            "但是和$p$l偏了几寸。",
            "但是被$p机灵地躲开了。",
            "但是$n身子一侧，闪了开去。",
            "但是被$p及时避开。",
            "但是$n已有准备，不慌不忙地躲开。",
        }
        return msgs[random(5) + 1]
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
