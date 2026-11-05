-- 防线5 NOTE形态：目标在另一脚本的返回表里（跨文件 owner 契约）——
-- 不判失败，提示人工核对 owner 的脚本确实定义它
local npc = {}

function npc.go(self)
    self:call_out("remote_cb", 1)
end

return npc
