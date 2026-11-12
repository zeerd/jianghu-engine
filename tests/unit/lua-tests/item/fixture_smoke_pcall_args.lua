-- 冒烟夹具：pcall 参数缺失——原 pcall 自己抛错，包装层"原 pcall 就炸"
-- 这条上抛路径同样要走到并记进日志。
return {
    type = "item",
    create = function(self, flag)
        pcall()
    end,
}
