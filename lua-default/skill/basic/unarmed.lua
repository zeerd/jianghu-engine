-- unarmed.lua  基本拳脚

return {
    create = function(self)
        self:set_name("基本拳脚")
    end,

    query_action = function(self, me)
        local actions = {
            "$N冲上去狠狠的打了$n一巴掌。",
            "$N挥起板砖，拍在了$n的破脑袋上。",
            "$N大喝一声，揪住$n的衣领往死里揍。",
            "$N不由分说，扬起一脚，踢在$n的小腹上。",
            "$N举起拳头，将$n的鼻子打开了花。",
            "$N扬起手指，痛苦的戳在了$n的嘴巴上。",
            "$N冲上去狠狠的踢了$n一脚。",
            "$N操起铁锹，狠狠的砸在$n的脑袋上。",
        }

        return { action = actions[random(8) + 1] }
    end,
}
