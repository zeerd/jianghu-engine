-- 冒烟夹具：require 上面的模块，用来证明「被 require 的文件跳过冒烟」。
local lib = require("item.fixture_smoke_lib")

return {
    type = "item",
    create = function(self, flag)
        if lib then
            return 1
        end
        return 0
    end,
}
