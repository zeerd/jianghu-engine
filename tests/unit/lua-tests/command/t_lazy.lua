-- t_lazy.lua — 命令模块夹具：验证 command/<命令名>.lua 的懒加载解析。
-- 约定：模块返回表，入口函数键名与文件名相同。
local M = {}

function M.t_lazy(me, target, arg)
    _G.__lazy_cap = { name = target, arg = arg }
    return 1
end

return M
