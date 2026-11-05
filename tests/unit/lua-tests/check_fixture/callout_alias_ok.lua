-- 防线5 正例：别名导出——local 函数 + 返回表里挂名，字段在表里即可按名注册
local function aliased_cb(self, p1, p2) end

return {
    go = function(self)
        self:call_out("aliased_cb", 1)
    end,
    aliased_cb = aliased_cb,
}
