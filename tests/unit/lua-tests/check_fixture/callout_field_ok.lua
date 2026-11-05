-- 防线5 正例：延迟函数是返回表字段（function 模块名.名字）
local npc = {}

function npc.show_cb(self, p1, p2) end

function npc.go(self)
    self:call_out("show_cb", 1)
end

return npc
