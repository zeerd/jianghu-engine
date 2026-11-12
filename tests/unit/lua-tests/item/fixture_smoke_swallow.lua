-- 冒烟夹具：错误被脚本自己的 pcall 吞掉——静态防线看不见，引擎日志里
-- 也一行都没有（是脚本自己接住的）。只有冒烟期的记录版 pcall 抓得到。
return {
    type = "item",
    create = function(self, flag)
        pcall(function()
            return self:no_such_method_xyz()
        end)
    end,
}
